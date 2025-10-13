using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using System.Collections.Concurrent;
using WebServer.Data;
using WebServer.Dtos;
using WebServer.Services;

namespace WebServer.Controllers;

[Authorize]
[ApiController]
[Route("api/[controller]")]
public class MatchmakingController : ControllerBase
{
    private readonly ICachingService _cachingService;
    private static readonly ConcurrentDictionary<string, ConcurrentBag<TaskCompletionSource<ValueTuple<string, ushort>>>> _waitingGroups = new();

    public MatchmakingController(ICachingService cachingService)
    {
        _cachingService = cachingService;
    }

    [HttpGet("checkstatus")]
    public async Task<IActionResult> CheckStatus([FromHeader] Guid groupId)
    {
        var bag = _waitingGroups.GetOrAdd(groupId.ToString(), []);
        var tcs = new TaskCompletionSource<(string, ushort)>(TaskCreationOptions.RunContinuationsAsynchronously);
        bag.Add(tcs);

        _waitingGroups[groupId.ToString()] = bag;

        try
        {
            var serverInfo = await tcs.Task;
            var response = new GroupStatusResponseDto()
            {
                Status = true,
                ServerIp = serverInfo.Item1,
                Port = serverInfo.Item2
            };

            return Ok(response);
        }
        catch (TaskCanceledException)
        {
            return Ok(new GroupStatusResponseDto { Status = false });
        }
    }

    [HttpPost("start")]
    public async Task<IActionResult> SignalGroupReady([FromHeader] Guid groupId, [FromBody] UserSimpleDto requester)
    {
        if (requester is null)
            return BadRequest();

        GroupDto? group = await _cachingService.GetGroupInfoInCacheAsync(groupId);

        if (group == null || group.Owner.Uid != requester.Uid)
            return BadRequest();

        if (_waitingGroups.TryRemove(groupId.ToString(), out var bag))
        {
            // TODO : Utility 등으로 빼야됨
            ValueTuple<string, ushort> serverInfo = ("127.0.0.1", 53200);

            foreach (var tcs in bag)
            {
                tcs.TrySetResult(serverInfo);
            }

            return Ok($"{bag.Count} clients signaled");
        }

        return NotFound("No clients waiting for this group");
    }
}