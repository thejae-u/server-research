using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Caching.Distributed;
using WebServer.Dtos;
using WebServer.Services;

namespace WebServer.Controllers;

[ApiController]
[Route("api/in-game")]
public class InGameController : ControllerBase
{
    private readonly IGroupService _groupService;
    private readonly IConfiguration _config;

    private readonly IDistributedCache _cache;

    public InGameController(IGroupService groupService, IConfiguration config, IDistributedCache cache)
    {
        _groupService = groupService;
        _config = config;
        _cache = cache;
    }

    [HttpPost("create")]
    public async Task<IActionResult> CreateNewGroup([FromBody] Guid userid)
    {
        var group = await _groupService.CreateNewGroupAsync(userid);

        return Ok(group);
    }

    [HttpGet("info/{groupid}")]
    public async Task<IActionResult> GetGroupInfoById(Guid groupid)
    {
        var group = await _groupService.GetGroupInfoAsync(groupid);
        return group == null ? NotFound() : Ok(group);
    }

    [HttpPost("join")]
    public async Task<IActionResult> JoinGroup([FromHeader] Guid groupid, Guid userid)
    {
        var group = await _groupService.JoinGroupAsync(groupid, userid);
        return group == null ? NotFound() : Ok(group);
    }

    [HttpPost("leave")]
    public async Task<IActionResult> LeaveGroup([FromHeader] Guid groupid, Guid userid)
    {
        var group = await _groupService.LeaveGroupAsync(groupid, userid);
        return group == null ? NotFound() : Ok(group);
    }

    [HttpDelete("{groupid}")]
    public async Task<IActionResult> DeleteGroup([FromHeader] Guid groupid)
    {
        await Task.Yield();
        return NoContent();
    }
}