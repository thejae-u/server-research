using System.Text.Json;
using WebServer.Data;
using WebServer.Dtos;
using WebServer.Utils;

namespace WebServer.Services;

public class GroupService : IGroupService
{
    private const int DEFAULT_MAX_PLAYER = 4;
    private readonly IUserService _userService;

    // Redis Caching Service
    private readonly ICachingService _cachingService;

    public GroupService(IUserService userService, ICachingService cachingService)
    {
        _userService = userService;
        _cachingService = cachingService;
    }

    public async Task<IEnumerable<GroupDto>> GetAllGroupAsync()
    {
        return await _cachingService.GetAllGroupInCacheAsync();
    }

    public async Task<GroupDto?> CreateNewGroupAsync(CreateGroupRequestDto createGroupRequestDto)
    {
        var groupName = createGroupRequestDto.GroupName;
        var requester = createGroupRequestDto.Requester;
        var newGroupId = Guid.NewGuid();
        var now = DateTime.UtcNow;
        var initialMembers = new List<UserSimpleDto> { requester };

        var newGroup = new GroupDto
        {
            GroupId = newGroupId,
            Name = groupName,
            Players = initialMembers,
            Owner = requester,
            CreatedAt = now
        };

        bool commited = await _cachingService.SetNewGroupAsync(newGroup);
        if (!commited)
            return null;

        return newGroup;
    }

    public async Task<Result<GroupDto?>> GetGroupInfoAsync(Guid groupId)
    {
        var groupInfo = await _cachingService.GetGroupInfoInCacheAsync(groupId);
        if (groupInfo is null)
            return Result<GroupDto?>.Failure("Failed Get Group Info");

        return Result<GroupDto?>.Success(groupInfo);
    }

    public async Task<Result<GroupDto?>> JoinGroupAsync(JoinGroupRequestDto requestDto)
    {
        var groupId = requestDto.GroupId;
        var requester = requestDto.Requester;
        var initialGroupInfo = await GetGroupInfoAsync(groupId);

        if (!initialGroupInfo.IsSuccess)
            return initialGroupInfo; // Failure result Return;

        var group = initialGroupInfo.Value;

        if (group == null)
        {
            await _cachingService.RemoveAsync(_cachingService.GetGroupKey(groupId));
            return Result<GroupDto?>.Failure("Invalid Group Info");
        }

        if (group.Players.Count == 0)
        {
            await _cachingService.RemoveAsync(_cachingService.GetGroupKey(groupId));
            return Result<GroupDto?>.Failure("Invalid Group Member");
        }

        if (group.Players.Any(p => p.Uid == requester.Uid))
        {
            return Result<GroupDto?>.Failure("Already exist user request");
        }

        if (group.Players.Count == DEFAULT_MAX_PLAYER)
            return Result<GroupDto?>.Failure("Group is Full");

        group.Players.Add(requester);

        bool commited = await _cachingService.JoinGroupInCacheAsync(group, requestDto);
        if (!commited)
            return Result<GroupDto?>.Failure("data race, try again");

        return Result<GroupDto?>.Success(group);
    }

    public async Task<Result<GroupDto?>> LeaveGroupAsync(DefaultGroupRequestDto requestDto)
    {
        var groupId = requestDto.GroupId;
        var userId = requestDto.UserId;

        var initialGroupResult = await GetGroupInfoAsync(groupId);
        if (!initialGroupResult.IsSuccess)
            return Result<GroupDto?>.Failure(initialGroupResult.Error!);

        var group = initialGroupResult.Value;
        if (group == null)
        {
            await _cachingService.RemoveAsync(_cachingService.GetGroupKey(groupId));
            return Result<GroupDto?>.Failure("Invalid Group Info");
        }

        var userToRemove = group.Players.FirstOrDefault(p => p.Uid == userId);
        if (userToRemove == null)
            return Result<GroupDto?>.Failure("User not exist in group");

        group.Players.Remove(userToRemove);

        bool committed = await _cachingService.LeaveGroupInCacheAsync(group, requestDto);
        if (!committed)
            return Result<GroupDto?>.Failure("data race. try again");

        return Result<GroupDto?>.Success(group.Players.Count > 0 ? group : null);
    }

    // Admin Method Area
    public async Task<bool> FlushGroupsAsync(UserDto requester)
    {
        var user = await _userService.GetUSerByIdAsync(requester.Uid);
        if (user == null)
        {
            return false;
        }

        if (!user.Role.Equals(requester.Role) || !user.Role.Equals(RoleCaching.Admin))
        {
            return false;
        }

        await _cachingService.FlushGroupKeysAsync();
        return true;
    }

    public async Task FlushGroupAsyncTest()
    {
        await _cachingService.FlushGroupKeysAsync();
    }
}