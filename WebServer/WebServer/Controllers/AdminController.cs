using Microsoft.AspNetCore.Mvc;
using WebServer.Data;
using WebServer.Dtos;
using WebServer.Services;

namespace WebServer.Controllers;

[ApiController]
[Route("api/admin")]
public class AdminController : ControllerBase
{
    private readonly IUserService _userService;
    private readonly IGroupService _groupService;
    private readonly IConfiguration _config;

    public AdminController(IUserService userService, IGroupService groupService, IConfiguration config)
    {
        _userService = userService;
        _groupService = groupService;
        _config = config;
    }

    [HttpGet("retrieve/users")]
    public async Task<IActionResult> GetAllUser()
    {
        var users = await _userService.GetAllUserAsync();
        return users.Count() == 0 ? NoContent() : Ok(users);
    }

    [HttpGet("retrieve/groups")]
    public async Task<IActionResult> GetAllGroup()
    {
        var groups = await _groupService.GetAllGroupAsync();
        return groups.Count() == 0 ? NoContent() : Ok(groups);
    }

    [HttpDelete("flush/groups")]
    public async Task<IActionResult> FlushAllCaching()
    {
        await _groupService.FlushGroupAsyncTest();
        return Ok();
    }

    [HttpPost("group/create")]
    public async Task<IActionResult> CreateGroupAdmin([FromBody] CreateGroupRequestDto request)
    {
        var group = await _groupService.CreateNewGroupAsync(request);
        return group is not null ? Ok(group) : BadRequest();
    }
}