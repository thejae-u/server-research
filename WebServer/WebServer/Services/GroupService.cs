using StackExchange.Redis;
using System.Text.Json;
using WebServer.Data;
using WebServer.Dtos;
using WebServer.Utils;

namespace WebServer.Services;

public class GroupService : IGroupService
{
    private const int DEFAULT_MAX_PLAYER = 4;
    private const double DEFAULT_EXPIRE_TIME = 5.0; // 5 hours

    private readonly IConnectionMultiplexer _redis;
    private readonly IDatabase _redisCache;
    private readonly IUserService _userService;

    private const string GroupKeyPrefix = "group:";

    private const string GroupNameField = "name";
    private const string OwnerUserField = "owner";
    private const string MembersField = "members";
    private const string CreatedAtField = "createdAt";

    public GroupService(IConnectionMultiplexer redis, IUserService userService)
    {
        _redis = redis;
        _redisCache = redis.GetDatabase();
        _userService = userService;
    }

    private string GetGroupKey(Guid groupId) => $"{GroupKeyPrefix}{groupId}";

    private async Task DeleteGroupKeyAsync(Guid groupId)
    {
        var groupKey = GetGroupKey(groupId);
        await _redisCache.KeyDeleteAsync(groupKey);
    }

    public async Task<IEnumerable<GroupDto>> GetAllGroupAsync()
    {
        var groups = new List<GroupDto>();
        var server = _redis.GetServer(_redis.GetEndPoints().First());

        await foreach (var key in server.KeysAsync(pattern: $"{GroupKeyPrefix}*"))
        {
            var groupIdString = key.ToString().Replace(GroupKeyPrefix, "");
            if (Guid.TryParse(groupIdString, out var groupId))
            {
                var result = await GetGroupInfoAsync(groupId);
                if (result.IsSuccess && result.Value != null)
                {
                    groups.Add(result.Value);
                }
            }
        }
        return groups;
    }

    public async Task<GroupDto?> CreateNewGroupAsync(CreateGroupRequestDto createGroupRequestDto)
    {
        var newGroupId = Guid.NewGuid();
        var groupkey = GetGroupKey(newGroupId);
        var now = DateTime.UtcNow;
        var requester = createGroupRequestDto.requester;
        var groupName = createGroupRequestDto.GroupName;

        var initialMembers = new List<UserSimpleDto> { requester };
        var membersJson = JsonSerializer.Serialize(initialMembers);
        var ownerUserJson = JsonSerializer.Serialize(requester);

        var groupData = new HashEntry[]
        {
            new(GroupNameField, groupName),
            new(CreatedAtField, now.ToString("O")),
            new(MembersField, membersJson),
            new(OwnerUserField, ownerUserJson)
        };

        var trans = _redisCache.CreateTransaction();
        _ = trans.HashSetAsync(groupkey, groupData);
        _ = trans.KeyExpireAsync(groupkey, TimeSpan.FromHours(DEFAULT_EXPIRE_TIME));

        bool commited = await trans.ExecuteAsync();
        if (!commited)
        {
            return null;
        }

        return new GroupDto
        {
            GroupId = newGroupId,
            Players = initialMembers,
            Name = groupName,
            Owner = requester,
            CreatedAt = now
        };
    }

    public async Task<Result<GroupDto?>> GetGroupInfoAsync(Guid groupId)
    {
        string groupKey = GetGroupKey(groupId);
        HashEntry[] groupData = await _redisCache.HashGetAllAsync(groupKey);

        if (groupData.Length == 0)
            return Result<GroupDto?>.Failure("Group does not exist");

        var groupDict = groupData.ToDictionary(x => x.Name.ToString(), x => x.Value);
        if (!groupDict.TryGetValue(MembersField, out var membersJson) ||
            !groupDict.TryGetValue(GroupNameField, out var groupName) ||
            !groupDict.TryGetValue(OwnerUserField, out var ownerJson) ||
            !groupDict.TryGetValue(CreatedAtField, out var createdAt))
        {
            _ = DeleteGroupKeyAsync(groupId);
            return Result<GroupDto?>.Failure("Group is invalid");
        }

        try
        {
            var members = JsonSerializer.Deserialize<List<UserSimpleDto>>(membersJson.ToString());
            var owner = JsonSerializer.Deserialize<UserSimpleDto>(ownerJson.ToString());

            if (members == null || owner == null)
            {
                _ = DeleteGroupKeyAsync(groupId);
                throw new ArgumentNullException("Json Serializer Error");
            }

            var group = new GroupDto
            {
                GroupId = groupId,
                Name = groupName.ToString(),
                Owner = owner,
                Players = members,
                CreatedAt = DateTime.Parse(createdAt.ToString()).ToUniversalTime()
            };

            return Result<GroupDto?>.Success(group);
        }
        catch (Exception ex)
        {
            _ = DeleteGroupKeyAsync(groupId);
            return Result<GroupDto?>.Failure($"Invalid Group {ex.Message}");
        }
    }

    public async Task<Result<GroupDto?>> JoinGroupAsync(JoinGroupRequestDto requestDto)
    {
        var groupId = requestDto.groupId;
        var requester = requestDto.requester;
        var initialGroupInfo = await GetGroupInfoAsync(groupId);

        if (!initialGroupInfo.IsSuccess)
            return initialGroupInfo; // Failure result Return;

        var group = initialGroupInfo.Value;

        if (group == null)
        {
            _ = DeleteGroupKeyAsync(groupId);
            return Result<GroupDto?>.Failure("Invalid Group Info");
        }

        if (group.Players.Count == 0)
        {
            _ = DeleteGroupKeyAsync(groupId);
            return Result<GroupDto?>.Failure("Invalid Group Member");
        }

        if (group.Players.Any(p => p.UID == requester.UID))
        {
            return Result<GroupDto?>.Failure("Already exist user request");
        }

        group.Players.Add(requester);
        var newMembersJson = JsonSerializer.Serialize(group.Players);

        var trans = _redisCache.CreateTransaction();
        var groupKey = GetGroupKey(groupId);
        trans.AddCondition(Condition.HashExists(groupKey, MembersField));
        _ = trans.HashSetAsync(groupKey, MembersField, newMembersJson);

        bool commited = await trans.ExecuteAsync();
        if (!commited)
            return Result<GroupDto?>.Failure("data race, try again");

        return Result<GroupDto?>.Success(group);
    }

    public async Task<Result<GroupDto?>> LeaveGroupAsync(DefaultGroupRequestDto requestDto)
    {
        var groupId = requestDto.groupId;
        var userId = requestDto.userId;

        var initialGroupResult = await GetGroupInfoAsync(groupId);
        if (!initialGroupResult.IsSuccess)
            return Result<GroupDto?>.Failure(initialGroupResult.Error!);

        var group = initialGroupResult.Value;
        if (group == null)
        {
            _ = DeleteGroupKeyAsync(groupId);
            return Result<GroupDto?>.Failure("Invalid Group Info");
        }

        var userToRemove = group.Players.FirstOrDefault(p => p.UID == userId);
        if (userToRemove == null)
            return Result<GroupDto?>.Failure("User not exist in group");

        group.Players.Remove(userToRemove);

        var trans = _redisCache.CreateTransaction();
        var groupKey = GetGroupKey(groupId);
        trans.AddCondition(Condition.HashExists(groupKey, MembersField));

        if (group.Players.Count == 0)
        {
            _ = trans.KeyDeleteAsync(groupKey);
        }
        else
        {
            var newMembersJson = JsonSerializer.Serialize(group.Players);
            _ = trans.HashSetAsync(groupKey, MembersField, newMembersJson);
        }

        bool committed = await trans.ExecuteAsync();
        if (!committed)
            return Result<GroupDto?>.Failure("data race. try again");

        return Result<GroupDto?>.Success(group.Players.Count > 0 ? group : null);
    }

    // Admin Method Area
    public async Task<bool> FlushGroupsAsync(UserDto requester)
    {
        var user = await _userService.GetUSerByIdAsync(requester.UID);
        if (user == null)
        {
            return false;
        }

        if (!user.Role.Equals(requester.Role) || !user.Role.Equals(RoleCaching.Admin))
        {
            return false;
        }

        var server = _redis.GetServer(_redis.GetEndPoints().First());
        var database = _redisCache;

        var patterns = new[]
        {
            $"{GroupKeyPrefix}*",
        };

        var deleteKeys = new List<RedisKey>();
        foreach (var pattern in patterns)
        {
            await foreach (var key in server.KeysAsync(pattern: pattern))
            {
                deleteKeys.Add(key);
            }
        }

        if (deleteKeys.Count != 0)
        {
            await database.KeyDeleteAsync(deleteKeys.ToArray());
        }

        return true;
    }

    public async Task<bool> FlushGroupAsyncTest()
    {
        var server = _redis.GetServer(_redis.GetEndPoints().First());
        var database = _redisCache;

        var patterns = new[]
        {
            $"{GroupKeyPrefix}*",
        };

        var deleteKeys = new List<RedisKey>();
        foreach (var pattern in patterns)
        {
            await foreach (var key in server.KeysAsync(pattern: pattern))
            {
                deleteKeys.Add(key);
            }
        }

        if (deleteKeys.Count != 0)
        {
            await database.KeyDeleteAsync(deleteKeys.ToArray());
        }

        return true;
    }
}