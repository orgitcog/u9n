// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Web;

#pragma warning disable CA2234 // Pass system uri objects instead of strings
#pragma warning disable CA1054 // URI-like parameters should not be strings

namespace EpicGames.Core
{
	/// <summary>
	/// Extension methods for consuming REST APIs via JSON objects
	/// </summary>
	public static class HttpClientExtensions
	{
		/// <summary>
		/// Gets a resource from an HTTP endpoint and parses it as a JSON object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to retrieve</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>New instance of the object</returns>
		public static async Task<TResponse> GetAsync<TResponse>(this HttpClient client, string url, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await client.GetAsync(url, cancellationToken))
			{
				response.EnsureSuccessStatusCode();
				return await ParseJsonContentAsync<TResponse>(response, cancellationToken);
			}
		}

		/// <inheritdoc cref="GetAsync{TResponse}(HttpClient, String, CancellationToken)"/>
		public static async Task<TResponse> GetAsync<TResponse>(this HttpClient client, Uri url, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await client.GetAsync(url, cancellationToken))
			{
				response.EnsureSuccessStatusCode();
				return await ParseJsonContentAsync<TResponse>(response, cancellationToken);
			}
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object
		/// </summary>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to post to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		public static async Task<HttpResponseMessage> PostAsync<TRequest>(this HttpClient client, string url, TRequest request, CancellationToken cancellationToken)
		{
			using HttpContent content = ToJsonContent(request);
			return await client.PostAsync(url, content, cancellationToken);
		}

		/// <inheritdoc cref="PostAsync{TRequest}(HttpClient, String, TRequest, CancellationToken)"/>
		public static async Task<HttpResponseMessage> PostAsync<TRequest>(this HttpClient client, Uri url, TRequest request, CancellationToken cancellationToken)
		{
			using HttpContent content = ToJsonContent(request);
			return await client.PostAsync(url, content, cancellationToken);
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to post to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		public static async Task<TResponse> PostAsync<TResponse, TRequest>(this HttpClient client, string url, TRequest request, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await PostAsync(client, url, request, cancellationToken))
			{
				response.EnsureSuccessStatusCode();
				return await ParseJsonContentAsync<TResponse>(response, cancellationToken);
			}
		}

		/// <inheritdoc cref="PostAsync{TRequest}(HttpClient, String, TRequest, CancellationToken)"/>
		public static async Task<TResponse> PostAsync<TResponse, TRequest>(this HttpClient client, Uri url, TRequest request, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await PostAsync(client, url, request, cancellationToken))
			{
				response.EnsureSuccessStatusCode();
				return await ParseJsonContentAsync<TResponse>(response, cancellationToken);
			}
		}

		/// <summary>
		/// Puts an object to an HTTP endpoint as a JSON object
		/// </summary>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to post to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		public static async Task<HttpResponseMessage> PutAsync<TRequest>(this HttpClient client, string url, TRequest request, CancellationToken cancellationToken)
		{
			using HttpContent content = ToJsonContent(request);
			return await client.PutAsync(url, content, cancellationToken);
		}

		/// <inheritdoc cref="PutAsync{TRequest}(HttpClient, String, TRequest, CancellationToken)"/>
		public static async Task<HttpResponseMessage> PutAsync<TRequest>(this HttpClient client, Uri url, TRequest request, CancellationToken cancellationToken)
		{
			using HttpContent content = ToJsonContent(request);
			return await client.PutAsync(url, content, cancellationToken);
		}

		/// <summary>
		/// Puts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to post to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		public static async Task<TResponse> PutAsync<TResponse, TRequest>(this HttpClient client, string url, TRequest request, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await PutAsync(client, url, request, cancellationToken))
			{
				response.EnsureSuccessStatusCode();
				return await ParseJsonContentAsync<TResponse>(response, cancellationToken);
			}
		}

		/// <inheritdoc cref="PutAsync{TRequest}(HttpClient, String, TRequest, CancellationToken)"/>
		public static async Task<TResponse> PutAsync<TResponse, TRequest>(this HttpClient client, Uri url, TRequest request, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await PutAsync(client, url, request, cancellationToken))
			{
				response.EnsureSuccessStatusCode();
				return await ParseJsonContentAsync<TResponse>(response, cancellationToken);
			}
		}

		/// <summary>
		/// Converts an object to a JSON http content object
		/// </summary>
		/// <typeparam name="T">Type of the object to parse</typeparam>
		/// <param name="obj">The object instance</param>
		/// <returns>Http content object</returns>
		private static HttpContent ToJsonContent<T>(T obj)
		{
			return new StringContent(JsonSerializer.Serialize<T>(obj), Encoding.UTF8, "application/json");
		}

		/// <summary>
		/// Parses a HTTP response as a JSON object
		/// </summary>
		/// <typeparam name="T">Type of the object to parse</typeparam>
		/// <param name="message">The message received</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Parsed object instance</returns>
		private static async Task<T> ParseJsonContentAsync<T>(HttpResponseMessage message, CancellationToken cancellationToken)
		{
			byte[] bytes = await message.Content.ReadAsByteArrayAsync(cancellationToken);
			return JsonSerializer.Deserialize<T>(bytes, new JsonSerializerOptions { PropertyNameCaseInsensitive = true })!;
		}
	}
	
	/// <summary>
	/// Extension methods for HttpRequestMessage
	/// </summary>
	public static class HttpRequestMessageExtensions
	{
		private static readonly HashSet<string> s_sensitiveKeys = new(StringComparer.OrdinalIgnoreCase)
		{
			"x-amz-security-token",
			"Signature",
			"AWSAccessKeyId"
		};
		
		/// <summary>
		/// Returns a new URI with sensitive query parameters redacted from the request URI.
		/// </summary>
		public static Uri? RedactedRequestUri(this HttpRequestMessage request)
		{
			Uri? uri = request?.RequestUri;
			if (uri?.Query is not { Length: > 0 })
			{
				return uri;
			}
			
			NameValueCollection queryParams = HttpUtility.ParseQueryString(uri.Query);
			foreach (string key in queryParams.AllKeys.OfType<string>())
			{
				string value = s_sensitiveKeys.Contains(key) ? "redacted" : queryParams[key] ?? String.Empty;
				queryParams[key] = value;
			}
			
			return new UriBuilder(uri) { Query = String.Join("&", queryParams) }.Uri;
		}
	}
}
