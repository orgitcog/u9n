// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text.Json;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;

namespace Jupiter.FunctionalTests.Auth
{
	[TestClass]
	public class AuthControllerTests : IDisposable
	{
		private HttpClient? _httpClient;
		private TestServer? _server;

		[TestInitialize]
		public async Task SetupAsync()
		{
			IConfigurationRoot configuration = new ConfigurationBuilder()
				// we are not reading the base appSettings here as we want exact control over what runs in the tests
				.AddJsonFile("appsettings.Testing.json", true)
				.AddInMemoryCollection(new[]
				{
					new KeyValuePair<string, string?>("Auth:ClientOidcEncryptionKey", TestEncryptionKey),
					new KeyValuePair<string, string?>("Auth:ClientOidcConfiguration:Providers:Foo:DisplayName", "Bar"),
				})
				.AddEnvironmentVariables()
				.Build();

			Logger logger = new LoggerConfiguration()
				.ReadFrom.Configuration(configuration)
				.CreateLogger();

			_server = new TestServer(new WebHostBuilder()
				.UseConfiguration(configuration)
				.UseEnvironment("Testing")
				.ConfigureServices(collection => collection.AddSerilog(logger))
				.UseStartup<JupiterStartup>()
			);
			_httpClient = _server.CreateClient();

			await Task.CompletedTask;
		}

		public string TestEncryptionKey { get; } = "dfc5c1ff52324677a2a49b66051ca7f1";

		[TestMethod]
		public async Task GetAuthConfigAsync()
		{
			HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/auth/oidc-configuration", UriKind.Relative));
			result.EnsureSuccessStatusCode();
			byte[] b = await result.Content.ReadAsByteArrayAsync();
			using Aes aes = Aes.Create();
			int ivLength = aes.IV.Length;
			byte[] iv = new byte[ivLength];

			Array.Copy(b, iv, ivLength);

			byte[] key = Convert.FromHexString(TestEncryptionKey);
			MemoryStream ms = new MemoryStream(b, ivLength, b.Length - ivLength);
			await using CryptoStream cryptoStream = new(ms, aes.CreateDecryptor(key, iv), CryptoStreamMode.Read);
			ClientOidcConfiguration? config = await JsonSerializer.DeserializeAsync<ClientOidcConfiguration>(cryptoStream);
			Assert.IsNotNull(config);
			Assert.AreEqual("Bar", config.Providers["Foo"].DisplayName);
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				_httpClient?.Dispose();
				_server?.Dispose();
			}
		}

		public void Dispose()
		{
			Dispose(true);
			System.GC.SuppressFinalize(this);
		}
	}
}
