// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.IO;
using System.Net.Mime;
using System.Security.Cryptography;
using System.Text.Json;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Jupiter.Controllers
{
	[ApiController]
	[Route("api/v1/auth")]
	[Authorize]
	public class AuthController : ControllerBase
	{
		private readonly IRequestHelper _requestHelper;
		private readonly IOptionsMonitor<AuthSettings> _authSettings;

		public AuthController(IRequestHelper requestHelper, IOptionsMonitor<AuthSettings> authSettings)
		{
			_requestHelper = requestHelper;
			_authSettings = authSettings;
		}

		[HttpGet("oidc-configuration")]
		// disable authentication on this endpoint
		[AllowAnonymous]
		// this endpoint always produces encrypted json
		[Produces(MediaTypeNames.Application.Octet)]
		public async Task<ActionResult> GetConfigAsync()
		{
			if (_authSettings.CurrentValue.ClientOidcConfiguration == null)
			{
				return BadRequest();
			}

			byte[] b = JsonSerializer.SerializeToUtf8Bytes(_authSettings.CurrentValue.ClientOidcConfiguration);

			using Aes aes = Aes.Create();
			byte[] key = Convert.FromHexString(_authSettings.CurrentValue.ClientOidcEncryptionKey);
			aes.Key = key;
			aes.GenerateIV();
			// write the IV into the stream before the encrypted content
			Stream responseStream = new MemoryStream();
			await responseStream.WriteAsync(aes.IV);
			await using CryptoStream cryptoStream = new(responseStream, aes.CreateEncryptor(), CryptoStreamMode.Write);
			await cryptoStream.WriteAsync(b, 0, b.Length);
			await cryptoStream.FlushFinalBlockAsync();

			Response.ContentType = MediaTypeNames.Application.Octet;
			Response.StatusCode = 200;
			Response.ContentLength = responseStream.Length;
			responseStream.Position = 0;
			await responseStream.CopyToAsync(Response.Body);
			return new EmptyResult();
		}

		[HttpGet("{ns}")]
		public async Task<IActionResult> VerifyAsync([FromRoute][Required] NamespaceId ns)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new[] { JupiterAclAction.ReadObject });
			if (result != null)
			{
				return result;
			}

			return Ok();
		}

		[HttpGet("{ns}/actions")]
		public IActionResult Actions([FromRoute][Required] NamespaceId ns)
		{
			List<JupiterAclAction> allowedActions = new List<JupiterAclAction>();
			
			CaptureLogger captureLogger = new CaptureLogger();
			bool policiesFound = false;
			foreach (AclPolicy acl in _authSettings.CurrentValue.Policies)
			{
				policiesFound = true;
				allowedActions.AddRange(acl.Resolve(User, new AccessScope(ns), captureLogger));
			}

			if (!policiesFound)
			{
				captureLogger.LogWarning("No policies set so no actions generated");
				return BadRequest(new JsonResult(new { Actions = allowedActions, LogOutput = captureLogger.RenderLines() }));
			}

			return Ok(new JsonResult(new { Actions = allowedActions , LogOutput = captureLogger.RenderLines()}));
		}
		
		[HttpGet("{ns}/{bucket}/actions")]
		public IActionResult Actions([FromRoute][Required] NamespaceId ns, [FromRoute][Required] BucketId bucket)
		{
			List<JupiterAclAction> allowedActions = new List<JupiterAclAction>();
			
			CaptureLogger captureLogger = new CaptureLogger();
			bool policiesFound = false;
			foreach (AclPolicy acl in _authSettings.CurrentValue.Policies)
			{
				policiesFound = true;
				allowedActions.AddRange(acl.Resolve(User, new AccessScope(ns, bucket), captureLogger));
			}

			if (!policiesFound)
			{
				captureLogger.LogWarning("No policies set so no actions generated");
				return BadRequest(new JsonResult(new { Actions = allowedActions, LogOutput = captureLogger.RenderLines() }));
			}

			return Ok(new JsonResult(new { Actions = allowedActions , LogOutput = captureLogger.RenderLines()}));
		}
	}

	public class ActionsResult
	{
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
		public List<JupiterAclAction> Actions { get; set; } = new List<JupiterAclAction>();
	}
}
