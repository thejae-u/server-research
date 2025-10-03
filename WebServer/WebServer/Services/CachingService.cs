using StackExchange.Redis;
using System.Text.Json;
using WebServer.Dtos;

namespace WebServer.Services;

public class CachingService : ICachingService
{
    private const double DEFAULT_EXPIRE_TIME = 5.0; // 5 hours

    private readonly IConnectionMultiplexer _redis;
    private readonly IDatabase _db;

    #region Fields For User

    private const string UserKeyPrefix = "user:";

    #endregion Fields For User

    #region Fields For Group

    private const string GroupKeyPrefix = "group:";

    private const string GroupNameField = "name";
    private const string OwnerUserField = "owner";
    private const string MembersField = "members";
    private const string CreatedAtField = "createdAt";

    #endregion Fields For Group

    public string GetGroupKey(Guid groupId) => $"{GroupKeyPrefix}{groupId}";

    public string GetUserKey(Guid userId) => $"{UserKeyPrefix}{userId}";

    public CachingService(IConnectionMultiplexer connectionMultiplexer)
    {
        _db = connectionMultiplexer.GetDatabase();
        _redis = connectionMultiplexer;
    }

    public async Task<T?> GetAsync<T>(string key)
    {
        var result = await _db.StringGetAsync(key);
        if (string.IsNullOrEmpty(result))
            return default;

        return JsonSerializer.Deserialize<T>(result!);
    }

    public async Task SetAsync<T>(string key, T value, TimeSpan? expiry = null)
    {
        var serializeData = JsonSerializer.Serialize(value);
        await _db.StringSetAsync(key, serializeData, expiry);
    }

    public async Task<bool> RemoveAsync(string key)
    {
        return await _db.KeyDeleteAsync(key);
    }

    #region User Methods

    public async Task<bool> SetUserLoginStatusAsync(Guid userId, TimeSpan? expiry = null)
    {
        var userKey = GetUserKey(userId);
        return await _db.StringSetAsync(userKey, "true", expiry);
    }

    public async Task<bool> IsUserLoggedInAsync(Guid userId)
    {
        return await _db.KeyExistsAsync(GetUserKey(userId));
    }

    public async Task<bool> ClearUserLoginStatusAsync(Guid userId)
    {
        return await _db.KeyDeleteAsync(GetUserKey(userId));
    }

    #endregion User Methods

    #region Group Method

    public async Task<IEnumerable<GroupDto>> GetAllGroupInCacheAsync()
    {
        var groups = new List<GroupDto>();
        var server = _redis.GetServer(_redis.GetEndPoints().First());

        await foreach (var key in server.KeysAsync(pattern: $"{GroupKeyPrefix}*"))
        {
            var groupIdString = key.ToString().Replace(GroupKeyPrefix, "");
            if (Guid.TryParse(groupIdString, out var groupId))
            {
                var result = await GetGroupInfoInCacheAsync(groupId);
                if (result is not null)
                {
                    groups.Add(result);
                }
            }
        }

        return groups;
    }

    public async Task<bool> SetNewGroupAsync(GroupDto newGroup)
    {
        var membersJson = JsonSerializer.Serialize(newGroup.Players);
        var ownerUserJson = JsonSerializer.Serialize(newGroup.Owner);

        var groupData = new HashEntry[]
        {
            new(GroupNameField, newGroup.Name),
            new(CreatedAtField, newGroup.CreatedAt.ToString("O")),
            new(MembersField, membersJson),
            new(OwnerUserField, ownerUserJson)
        };

        var groupKey = GetGroupKey(newGroup.GroupId);
        var trans = _db.CreateTransaction();
        _ = trans.HashSetAsync(groupKey, groupData);
        _ = trans.KeyExpireAsync(groupKey, TimeSpan.FromHours(DEFAULT_EXPIRE_TIME));

        return await trans.ExecuteAsync();
    }

    public async Task<bool> JoinGroupInCacheAsync(GroupDto group, JoinGroupRequestDto requester)
    {
        var newMembersJson = JsonSerializer.Serialize(group.Players);
        var trans = _db.CreateTransaction();
        var groupKey = GetGroupKey(group.GroupId);
        trans.AddCondition(Condition.HashExists(groupKey, MembersField));
        _ = trans.HashSetAsync(groupKey, MembersField, newMembersJson);

        return await trans.ExecuteAsync();
    }

    public async Task<bool> LeaveGroupInCacheAsync(GroupDto group, DefaultGroupRequestDto requester)
    {
        var trans = _db.CreateTransaction();
        var groupKey = GetGroupKey(group.GroupId);
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

        return await trans.ExecuteAsync();
    }

    public async Task FlushGroupKeysAsync()
    {
        var server = _redis.GetServer(_redis.GetEndPoints().First());

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
            await _db.KeyDeleteAsync(deleteKeys.ToArray());
        }
    }

    public async Task<GroupDto?> GetGroupInfoInCacheAsync(Guid groupId)
    {
        string groupKey = GetGroupKey(groupId);
        HashEntry[] groupData = await _db.HashGetAllAsync(groupKey);

        if (groupData.Length == 0)
            return null;

        var groupDict = groupData.ToDictionary(x => x.Name.ToString(), x => x.Value);
        if (!groupDict.TryGetValue(MembersField, out var membersJson) ||
            !groupDict.TryGetValue(GroupNameField, out var groupName) ||
            !groupDict.TryGetValue(OwnerUserField, out var ownerJson) ||
            !groupDict.TryGetValue(CreatedAtField, out var createdAt))
        {
            await RemoveAsync(groupKey);
            return null;
        }

        try
        {
            var members = JsonSerializer.Deserialize<List<UserSimpleDto>>(membersJson.ToString());
            var owner = JsonSerializer.Deserialize<UserSimpleDto>(ownerJson.ToString());

            if (members == null || owner == null)
            {
                await RemoveAsync(groupKey);
                return null;
            }

            var group = new GroupDto
            {
                GroupId = groupId,
                Name = groupName.ToString(),
                Owner = owner,
                Players = members,
                CreatedAt = DateTime.Parse(createdAt.ToString()).ToUniversalTime()
            };

            return group;
        }
        catch (Exception)
        {
            await RemoveAsync(groupKey);
            return null;
        }
    }

    #endregion Group Method
}