using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using WebServer.Data;
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

    [HttpGet]
    public async Task<IActionResult> GetAllPlayer()
    {
        var users = await _userService.GetAllUserAsync();
        return users.Any() ? Ok(users) : NoContent();
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
        var result = await _userService.DeleteUserAsync(username, userDeleteDto);
        return result ? Ok(new { Message = $"{userDeleteDto.UID} 탈퇴 처리 완료" }) : BadRequest();
    }
}