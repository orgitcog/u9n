// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.Json;
using EpicGames.Horde.Secrets;
using Microsoft.Extensions.Options;

namespace HordeServer.Secrets
{
	/// <summary>
	/// Implementation of <see cref="ISecretCollection"/>
	/// </summary>
	public class SecretCollection : ISecretCollection
	{
		readonly SecretCollectionInternal _secretCollectionInternal;
		readonly IOptionsSnapshot<SecretsConfig> _secretsConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public SecretCollection(SecretCollectionInternal secretCollectionInternal, IOptionsSnapshot<SecretsConfig> secretsConfig)
		{
			_secretCollectionInternal = secretCollectionInternal;
			_secretsConfig = secretsConfig;
		}

		/// <inheritdoc/>
		public async Task<ISecret?> GetAsync(SecretId secretId, CancellationToken cancellationToken)
		{
			SecretConfig? secretConfig;
			if (!_secretsConfig.Value.TryGetSecret(secretId, out secretConfig))
			{
				return null;
			}

			// secret provider configs are optional
			Dictionary<string, SecretProviderConfig>? secretProviderConfigs;
			_secretsConfig.Value.TryGetSecretProviderConfigs(secretId, out secretProviderConfigs);

			return await _secretCollectionInternal.GetAsync(secretConfig, secretProviderConfigs, cancellationToken);
		}
	}

	/// <summary>
	/// Core implementation of <see cref="ISecretCollection"/> which exists as a global service.
	/// </summary>
	public class SecretCollectionInternal
	{
		class Secret : ISecret
		{
			public SecretId Id { get; }
			public IReadOnlyDictionary<string, string> Data { get; }

			public Secret(SecretId id, IReadOnlyDictionary<string, string> data)
			{
				Id = id;
				Data = data;
			}
		}

		readonly Dictionary<string, ISecretProvider> _providers;

		/// <summary>
		/// Constructor
		/// </summary>
		public SecretCollectionInternal(IEnumerable<ISecretProvider> providers)
		{
			_providers = providers.ToDictionary(x => x.Name, x => x);
		}

		/// <summary>
		/// Resolve a secret to concrete values
		/// </summary>
		/// <param name="config">Configuration for the secret</param>
		/// <param name="secretProviderConfigs">Secret provider configs for the secret</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<ISecret> GetAsync(SecretConfig config, Dictionary<string, SecretProviderConfig>? secretProviderConfigs, CancellationToken cancellationToken)
		{
			// Add the hard-coded secrets
			Dictionary<string, string> data = new Dictionary<string, string>();
			foreach ((string key, string value) in config.Data)
			{
				data.Add(key, value);
			}

			// Fetch all the secrets from external providers
			foreach (ExternalSecretConfig externalConfig in config.Sources)
			{
				ISecretProvider provider;
				SecretProviderConfig? providerConfig = null;
				if (!String.IsNullOrEmpty(externalConfig.Provider))
				{
					provider = _providers[externalConfig.Provider];
				}
				else if (secretProviderConfigs != null && secretProviderConfigs.TryGetValue(externalConfig.ProviderConfig, out providerConfig) && !String.IsNullOrEmpty(providerConfig.Provider))
				{
					provider = _providers[providerConfig.Provider];
				}
				else
				{
					throw new InvalidOperationException($"Missing or invalid secret provider identifier for '{externalConfig.ProviderConfig}'");
				}

				string secret = await provider.GetSecretAsync(externalConfig.Path, providerConfig, cancellationToken);
				if (externalConfig.Format == ExternalSecretFormat.Text)
				{
					data[externalConfig.Key ?? "default"] = secret;
				}
				else if (externalConfig.Format == ExternalSecretFormat.Json)
				{
					Dictionary<string, string>? values = JsonSerializer.Deserialize<Dictionary<string, string>>(secret, new JsonSerializerOptions { AllowTrailingCommas = true });
					if (values != null)
					{
						foreach ((string key, string value) in values)
						{
							data[key] = value;
						}
					}
				}
				else
				{
					throw new NotImplementedException();
				}
			}

			return new Secret(config.Id, data);
		}
	}
}
