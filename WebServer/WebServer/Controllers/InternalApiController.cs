using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Configuration;
using Microsoft.IdentityModel.Tokens;
using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;
using System.Text;
using WebServer.Utils;

namespace WebServer.Controllers;

[ApiController]
[Route("api/internal")]
public class InternalApiController : ControllerBase
{
    private readonly IConfiguration _config;

    public InternalApiController(IConfiguration config)
    {
        _config = config;
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

        // jwt 값 가져오기
        var jwtSettings = TokenUtils.GetJwtSettings(_config);

        // 토큰 검증 규칙 생성
        var validationParameters = TokenUtils.GetTokenValidationParam(jwtSettings);

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