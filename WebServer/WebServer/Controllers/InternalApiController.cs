using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.IdentityModel.Tokens;
using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;

using WebServer.Services;

namespace WebServer.Controllers;

[Authorize]
[ApiController]
[Route("api/internal")]
public class InternalApiController : ControllerBase
{
    private readonly IConfiguration _config;
    private readonly ITokenService _tokenService;

    public InternalApiController(IConfiguration config, ITokenService tokenService)
    {
        _config = config;
        _tokenService = tokenService;
    }

    [HttpGet("validate-token")]
    public IActionResult ValidateToken([FromBody] TokenValidateRequest request)
    {
        const string apiKeyHeaderName = "X-Internal-API-Key";
        if (!Request.Headers.TryGetValue(apiKeyHeaderName, out var receivedApiKey))
        {
            return Unauthorized("API Key is missing");
        }

        var secretKey = _config["InternalApi:SecretKey"];
        if (secretKey != receivedApiKey)
        {
            return Unauthorized("Invalid API Key");
        }

        // 토큰 검증 규칙 생성
        var validationParameters = _tokenService.GetTokenValidationParam();

        // 토큰 검증
        var tokenHandler = new JwtSecurityTokenHandler();
        try
        {
            var principal = tokenHandler.ValidateToken(request.Token, validationParameters, out SecurityToken validatedToken);
            var userId = principal.FindFirst(ClaimTypes.NameIdentifier)?.Value;

            if (string.IsNullOrEmpty(userId))
            {
                return Unauthorized("Token does not contain a user ID");
            }

            return Ok(new { IsValid = true, UserId = userId });
        }
        catch (Exception ex)
        {
            return Unauthorized(new { IsValid = false, Error = "Token validation failed.", Details = ex.Message });
        }
    }
}