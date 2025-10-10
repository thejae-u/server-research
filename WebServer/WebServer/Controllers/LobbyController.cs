using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Caching.Distributed;
using WebServer.Dtos;
using WebServer.Services;

namespace WebServer.Controllers;

[Authorize]
[ApiController]
[Route("api/group")]
public class LobbyController : ControllerBase
{
    private readonly IGroupService _groupService;
    private readonly IConfiguration _config;

    private readonly IDistributedCache _cache;

    public LobbyController(IGroupService groupService, IConfiguration config, IDistributedCache cache)
    {
        _groupService = groupService;
        _config = config;
        _cache = cache;
    }

    [HttpGet("info/lobby")]
    public async Task<IActionResult> GetAllGroupInfoForLobby()
    {
        var groups = await _groupService.GetAllGroupAsync();
        return groups.Count() == 0 ? NoContent() : Ok(groups);
    }

    [HttpPost("create")]
    public async Task<IActionResult> CreateNewGroup([FromBody] CreateGroupRequestDto groupCreateDto)
    {
        var group = await _groupService.CreateNewGroupAsync(groupCreateDto);

        return Ok(group);
    }

    [HttpGet("info")]
    public async Task<IActionResult> GetGroupInfoById([FromBody] Guid groupId)
    {
        Utils.Result<GroupDto?> group = await _groupService.GetGroupInfoAsync(groupId);
        if (group is null || !group.IsSuccess) return NotFound();

        return Ok(group.Value);
    }

    [HttpPost("join")]
    public async Task<IActionResult> JoinGroup([FromBody] JoinGroupRequestDto requestDto)
    {
        Utils.Result<GroupDto?> group = await _groupService.JoinGroupAsync(requestDto);
        if (group is null || !group.IsSuccess) return NotFound();

        return Ok(group.Value);
    }

    [HttpPost("leave")]
    public async Task<IActionResult> LeaveGroup([FromBody] DefaultGroupRequestDto requestDto)
    {
        Utils.Result<GroupDto?> group = await _groupService.LeaveGroupAsync(requestDto);
        if (group is null || !group.IsSuccess) return NotFound();

        return Ok(group.Value);
    }
}