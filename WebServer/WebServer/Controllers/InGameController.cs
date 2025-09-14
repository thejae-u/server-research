using AutoMapper;
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
    private readonly IMapper _mapper;
    private readonly IConfiguration _config;

    private readonly IDistributedCache _cache;

    public InGameController(IGroupService groupService, IMapper mapper, IConfiguration config, IDistributedCache cache)
    {
        _groupService = groupService;
        _mapper = mapper;
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

    // 새 게임을 시작
    [HttpPost("start/{groupid}")]
    public async Task<IActionResult> CreateNewGame(Guid groupid)
    {
        await Task.Yield();
        // 로비 그룹 -> 게임 그룹
        return NoContent();
    }

    // 게임을 종료
    [HttpPost("close/{groupid}")]
    public async Task<IActionResult> CloseGame(Guid groupid)
    {
        await Task.Yield();
        // 게임 저장 로직, 게임 그룹 -> 로비 그룹
        return NoContent();
    }
}