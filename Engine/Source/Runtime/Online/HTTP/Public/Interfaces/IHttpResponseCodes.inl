// Copyright Epic Games, Inc. All Rights Reserved.

// status code not set yet
HTTP_RESPONSE_CODE(Unknown, 0)
// the request can be continued.
HTTP_RESPONSE_CODE(Continue, 100)
// the server has switched protocols in an upgrade header.
HTTP_RESPONSE_CODE(SwitchProtocol, 101)
// the request completed successfully.
HTTP_RESPONSE_CODE(Ok, 200)
// the request has been fulfilled and resulted in the creation of a new resource.
HTTP_RESPONSE_CODE(Created, 201)
// the request has been accepted for processing, but the processing has not been completed.
HTTP_RESPONSE_CODE(Accepted, 202)
// the returned meta information in the entity-header is not the definitive set available from the origin server.
HTTP_RESPONSE_CODE(Partial, 203)
// the server has fulfilled the request, but there is no new information to send back.
HTTP_RESPONSE_CODE(NoContent, 204)
// the request has been completed, and the client program should reset the document view that caused the request to be sent to allow the user to easily initiate another input action.
HTTP_RESPONSE_CODE(ResetContent, 205)
// the server has fulfilled the partial get request for the resource.
HTTP_RESPONSE_CODE(PartialContent, 206)
// the server couldn't decide what to return.
HTTP_RESPONSE_CODE(Ambiguous, 300)
// the requested resource has been assigned to a new permanent uri (uniform resource identifier), and any future references to this resource should be done using one of the returned uris.
HTTP_RESPONSE_CODE(Moved, 301)
// the requested resource resides temporarily under a different uri (uniform resource identifier).
HTTP_RESPONSE_CODE(Redirect, 302)
// the response to the request can be found under a different uri (uniform resource identifier) and should be retrieved using a get http verb on that resource.
HTTP_RESPONSE_CODE(RedirectMethod, 303)
// the requested resource has not been modified.
HTTP_RESPONSE_CODE(NotModified, 304)
// the requested resource must be accessed through the proxy given by the location field.
HTTP_RESPONSE_CODE(UseProxy, 305)
// the redirected request keeps the same http verb. http/1.1 behavior.
HTTP_RESPONSE_CODE(RedirectKeepVerb, 307)
// the request could not be processed by the server due to invalid syntax.
HTTP_RESPONSE_CODE(BadRequest, 400)
// the requested resource requires user authentication.
HTTP_RESPONSE_CODE(Denied, 401)
// not currently implemented in the http protocol.
HTTP_RESPONSE_CODE(PaymentReq, 402)
// the server understood the request, but is refusing to fulfill it.
HTTP_RESPONSE_CODE(Forbidden, 403)
// the server has not found anything matching the requested uri (uniform resource identifier).
HTTP_RESPONSE_CODE(NotFound, 404)
// the http verb used is not allowed.
HTTP_RESPONSE_CODE(BadMethod, 405)
// no responses acceptable to the client were found.
HTTP_RESPONSE_CODE(NoneAcceptable, 406)
// proxy authentication required.
HTTP_RESPONSE_CODE(ProxyAuthReq, 407)
// the server timed out waiting for the request.
HTTP_RESPONSE_CODE(RequestTimeout, 408)
// the request could not be completed due to a conflict with the current state of the resource. the user should resubmit with more information.
HTTP_RESPONSE_CODE(Conflict, 409)
// the requested resource is no longer available at the server, and no forwarding address is known.
HTTP_RESPONSE_CODE(Gone, 410)
// the server refuses to accept the request without a defined content length.
HTTP_RESPONSE_CODE(LengthRequired, 411)
// the precondition given in one or more of the request header fields evaluated to false when it was tested on the server.
HTTP_RESPONSE_CODE(PrecondFailed, 412)
// the server is refusing to process a request because the request entity is larger than the server is willing or able to process.
HTTP_RESPONSE_CODE(RequestTooLarge, 413)
// the server is refusing to service the request because the request uri (uniform resource identifier) is longer than the server is willing to interpret.
HTTP_RESPONSE_CODE(UriTooLong, 414)
// the server is refusing to service the request because the entity of the request is in a format not supported by the requested resource for the requested method.
HTTP_RESPONSE_CODE(UnsupportedMedia, 415)
// too many requests, the server is throttling
HTTP_RESPONSE_CODE(TooManyRequests, 429)
// the request should be retried after doing the appropriate action.
HTTP_RESPONSE_CODE(RetryWith, 449)
// the server encountered an unexpected condition that prevented it from fulfilling the request.
HTTP_RESPONSE_CODE(ServerError, 500)
// the server does not support the functionality required to fulfill the request.
HTTP_RESPONSE_CODE(NotSupported, 501)
// the server, while acting as a gateway or proxy, received an invalid response from the upstream server it accessed in attempting to fulfill the request.
HTTP_RESPONSE_CODE(BadGateway, 502)
// the service is temporarily overloaded.
HTTP_RESPONSE_CODE(ServiceUnavail, 503)
// the request was timed out waiting for a gateway.
HTTP_RESPONSE_CODE(GatewayTimeout, 504)
// the server does not support, or refuses to support, the http protocol version that was used in the request message.
HTTP_RESPONSE_CODE(VersionNotSup, 505)
