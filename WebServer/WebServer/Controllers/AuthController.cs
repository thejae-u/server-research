using Microsoft.AspNetCore.Mvc;
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

    [HttpGet("find/{userid}")]
    public async Task<IActionResult> GetUserById(Guid userid)
    {
        var user = await _userService.GetUSerByIdAsync(userid);
        return user != null ? Ok(user) : NotFound();
    }

    [HttpPost("register")]
    public async Task<IActionResult> Register([FromBody] UserRegisterDto userRegisterDto)
    {
        var userDto = await _userService.RegisterAsync(userRegisterDto);

        return userDto != null ? Ok(new { Message = "회원가입 성공!" }) : BadRequest();
    }

    [HttpPost("login")]
    public async Task<IActionResult> Login([FromBody] UserLoginDto userLoginDto)
    {
        var response = await _userService.LoginAsync(userLoginDto);
        return response != null ? Ok(response) : BadRequest();
    }

    [HttpDelete("cancellation")]
    public async Task<IActionResult> Cancellation([FromBody] UserDeleteDto userDeleteDto)
    {
        var result = await _userService.DeleteUserAsync(userDeleteDto);
        return result ? Ok(new { Message = $"탈퇴 처리 완료" }) : BadRequest();
    }
}