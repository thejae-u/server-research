using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
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

    [HttpGet("internal-validate-token")]
    public IActionResult ValidateToken()
    {
        var userId = User.FindFirstValue(ClaimTypes.NameIdentifier);
        return Ok(new { Message = "Token is valid", UserId = userId });
    }
}