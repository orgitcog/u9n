// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

namespace EpicGames.OIDC;

/// <summary>
/// An in-memory token store without encryption, intended for testing
/// </summary>
// ReSharper disable once ClassNeverInstantiated.Global
public class InMemoryTokenStore : ITokenStore
{
	private readonly Dictionary<string, string> _providerToRefreshToken = new ();

	/// <inheritdoc/>
	public virtual bool TryGetRefreshToken(string oidcProvider, out string refreshToken)
	{
		if (!_providerToRefreshToken.TryGetValue(oidcProvider, out string? result))
		{
			refreshToken = "";
			return false;
		}

		refreshToken = result;
		return true;
	}

	/// <inheritdoc/>
	public virtual void AddRefreshToken(string providerIdentifier, string refreshToken)
	{
		_providerToRefreshToken[providerIdentifier] = refreshToken;
	}

	/// <inheritdoc/>
	public virtual void Save()
	{
		// No need - data exists only in-memory
	}

	/// <summary>
	/// Dispose method
	/// </summary>
	/// <param name="disposing"></param>
	protected virtual void Dispose(bool disposing)
	{
		if (disposing)
		{
			// no resources to release
		}
	}

	/// <inheritdoc/>
	public void Dispose()
	{
		Dispose(true);
		GC.SuppressFinalize(this);
	}
}
