using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using System.Reflection.Metadata.Ecma335;
using System.Security.Claims;
using WebServer.Dtos;
using WebServer.Services;

namespace WebServer.Controllers;

[ApiController]
[Route("api/auth")]
public class AuthController : ControllerBase
{
    private readonly IUserService _userService;

    public AuthController(IUserService userService)
    {
        _userService = userService;
    }

    [Authorize]
    [HttpGet("find")]
    public async Task<IActionResult> GetUserById([FromBody] Guid userid)
    {
        var user = await _userService.GetUSerByIdAsync(userid);
        return user is not null ? Ok(user) : NotFound();
    }

    [HttpPost("register")]
    public async Task<IActionResult> Register([FromBody] UserRegisterDto userRegisterDto)
    {
        var userDto = await _userService.RegisterAsync(userRegisterDto);
        return userDto is not null ? Ok(new { Message = "회원가입 성공!" }) : BadRequest();
    }

    [HttpPost("login")]
    public async Task<IActionResult> Login([FromBody] UserLoginDto userLoginDto)
    {
        var response = await _userService.LoginAsync(userLoginDto);
        return response is not null ? Ok(response) : BadRequest();
    }

    [HttpPost("internal-login")]
    public async Task<IActionResult> InternalLogin([FromBody] UserLoginDto userLoginDto)
    {
        var response = await _userService.LoginInternalAsync(userLoginDto);
        return response is not null ? Ok(response) : BadRequest();
    }

    [Authorize]
    [HttpPost("logout")]
    public async Task<IActionResult> Logout([FromBody] UserSimpleDto userLogoutDto)
    {
        var response = await _userService.LogoutAsync(userLogoutDto);
        return response ? Ok(new { Message = "로그아웃 성공" }) : BadRequest();
    }

    [HttpPost("refresh")]
    public async Task<IActionResult> Refresh([FromBody] string refreshToken)
    {
        var response = await _userService.RefreshAsync(refreshToken);
        return response is not null ? Ok(response) : BadRequest();
    }

    [Authorize]
    [HttpDelete("cancellation")]
    public async Task<IActionResult> Cancellation([FromBody] UserDeleteDto userDeleteDto)
    {
        var result = await _userService.DeleteUserAsync(userDeleteDto);
        return result ? Ok(new { Message = $"탈퇴 처리 완료" }) : BadRequest();
    }

    [Authorize]
    [HttpGet("validate-token")]
    public IActionResult Validate()
    {
        var userId = User.FindFirstValue(ClaimTypes.NameIdentifier);
        return Ok(new { Message = "Token is valid", UserId = userId });
    }
}