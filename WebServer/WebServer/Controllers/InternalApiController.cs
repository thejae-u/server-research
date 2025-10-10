using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using WebServer.Dtos.InternalDto;
using WebServer.Services;

namespace WebServer.Controllers;

[Authorize(Roles = "Admin, Internal")]
[ApiController]
[Route("api/internal")]
public class InternalApiController : ControllerBase
{
    private readonly ITokenService _tokenService;

    public InternalApiController(ITokenService tokenService)
    {
        _tokenService = tokenService;
    }

    [HttpPost("save-game")]
    public async Task<IActionResult> SaveGame([FromBody] GameSaveDto saveDto)
    {
        await Task.Yield();
        return NoContent();
    }
}