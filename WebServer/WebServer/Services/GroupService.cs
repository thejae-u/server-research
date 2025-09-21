using StackExchange.Redis;
using WebServer.Dtos;

namespace WebServer.Services;

public class GroupService : IGroupService
{
    private const int ExpireTime = 1; // 그룹 Key 만료 시간 (Hours)

    private readonly IConnectionMultiplexer _redis;
    private readonly IDatabase _redisCache;

    private const string GroupMemberKeyPrefix = "group:members:";
    private const string GroupInfoExcludedMembersKeyPrefix = "group:info:";

    private const string CreatedAtStr = "createdAt";
    private const string AverageRttStr = "averageRtt";
    private const string AverageErrorRateStr = "averageErrorRate";

    public GroupService(IConnectionMultiplexer redis)
    {
        _redis = redis;
        _redisCache = redis.GetDatabase();
    }

    public async Task<UserGroupDto> CreateNewGroupAsync(Guid userId)
    {
        // Change to GroupData
        var newGroup = new UserGroupDto
        {
            GroupId = Guid.NewGuid(),
            Players = [userId],
            CreatedAt = DateTime.UtcNow,
            AverageRtt = 0,
            AverageErrorRate = 0
        };

        var infoKey = $"{GroupInfoExcludedMembersKeyPrefix}{newGroup.GroupId}";
        var memberKey = $"{GroupMemberKeyPrefix}{newGroup.GroupId}";

        var trans = _redisCache.CreateTransaction();

        if (trans == null)
            throw new ArgumentException(nameof(trans));

        _ = trans.HashSetAsync(infoKey,
        [
            new(CreatedAtStr, newGroup.CreatedAt.ToString("O")),
            new(AverageRttStr, newGroup.AverageRtt),
            new(AverageErrorRateStr, newGroup.AverageErrorRate)
        ]);

        _ = trans.SetAddAsync(memberKey, userId.ToString());

        var expiry = TimeSpan.FromHours(ExpireTime);
        _ = trans.KeyExpireAsync(infoKey, expiry);
        _ = trans.KeyExpireAsync(memberKey, expiry);

        bool commited = await trans.ExecuteAsync();
        if (!commited)
            throw new ArgumentException(nameof(commited));

        return newGroup;
    }

    public async Task<UserGroupDto?> GetGroupInfoAsync(Guid groupId)
    {
        var infoKey = $"{GroupInfoExcludedMembersKeyPrefix}{groupId}";
        var memberKey = $"{GroupMemberKeyPrefix}{groupId}";

        var infoTask = _redisCache.HashGetAllAsync(infoKey);
        var memberTask = _redisCache.SetMembersAsync(memberKey);

        await Task.WhenAll(infoTask, memberTask);

        var groupInfoEntries = infoTask.Result;
        var groupMembers = memberTask.Result;

        if (groupInfoEntries.Length == 0)
        {
            return null;
        }

        var groupInfo = groupInfoEntries.ToDictionary(
            entry => entry.Name.ToString(),
            entry => entry.Value
        );

        return new UserGroupDto
        {
            GroupId = groupId,
            Players = [.. groupMembers.Select(m => Guid.Parse(m.ToString()))],
            CreatedAt = DateTime.Parse(groupInfo[CreatedAtStr].ToString()).ToUniversalTime(),
            AverageRtt = (double)groupInfo[AverageRttStr],
            AverageErrorRate = (double)groupInfo[AverageErrorRateStr]
        };
    }

    public async Task<UserGroupDto?> JoinGroupAsync(Guid groupId, Guid userId)
    {
        const int MAX_PLAYER = 5; // 임시 최대 인원 설정

        var infoKey = $"{GroupInfoExcludedMembersKeyPrefix}{groupId}";
        var memberKey = $"{GroupMemberKeyPrefix}{groupId}";

        // 그룹 정보 redis로부터 읽음
        var groupInfoEntries = await _redisCache.HashGetAllAsync(infoKey);
        if (groupInfoEntries.Length == 0)
            return null; // No Match Group

        // Join 절차 시작
        var groupInfo = groupInfoEntries.ToDictionary(x => x.Name.ToString(), x => x.Value);

        var trans = _redisCache.CreateTransaction();
        if (trans == null)
            throw new Exception("Faile to Create Transaction.");

        // 데이터 레이스 방지 컨디션 설정
        trans.AddCondition(Condition.KeyExists(infoKey)); // 그룹이 유효
        trans.AddCondition(Condition.SetLengthLessThan(memberKey, MAX_PLAYER)); // 멤버 수가 최대를 넘지 않음
        trans.AddCondition(Condition.SetNotContains(memberKey, userId.ToString())); // 그룹에 속해 있지 않아야 함

        // Condition이 모두 참일 경우 실행
        _ = trans.SetAddAsync(memberKey, userId.ToString());

        bool commited = await trans.ExecuteAsync();
        if (!commited) // 실패 시 예외 처리
            throw new InvalidOperationException("Failed to join group. The group may be full, or the user is already a member.");

        return await GetGroupInfoAsync(groupId);
    }

    public async Task<UserGroupDto?> LeaveGroupAsync(Guid groupId, Guid userId)
    {
        await Task.Yield();
        return null;
    }

    // Admin Method Area

    public async Task<IEnumerable<UserGroupDto>> GetAllGroupAsync()
    {
        var groups = new List<UserGroupDto>();
        var server = _redis.GetServer(_redis.GetEndPoints().First());
        var pattern = $"{GroupInfoExcludedMembersKeyPrefix}*";

        await foreach (var key in server.KeysAsync(pattern: pattern))
        {
            var groupIdString = key.ToString().Split(":").Last();
            if (Guid.TryParse(groupIdString, out var groupId))
            {
                var groupInfo = await GetGroupInfoAsync(groupId);
                if (groupInfo != null)
                {
                    groups.Add(groupInfo);
                }
            }
        }

        return groups;
    }

    public async Task FlushCaching()
    {
        var server = _redis.GetServer(_redis.GetEndPoints().First());
        await server.FlushAllDatabasesAsync();
    }
}