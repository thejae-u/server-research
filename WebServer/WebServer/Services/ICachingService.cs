using WebServer.Dtos;

namespace WebServer.Services;

public interface ICachingService
{
    Task<T?> GetAsync<T>(string key);

    Task SetAsync<T>(string key, T value, TimeSpan? expiry = null);

    Task<bool> RemoveAsync(string key);

    #region Group Methods

    string GetGroupKey(Guid groupId);

    Task<IEnumerable<GroupDto>> GetAllGroupInCacheAsync();

    Task<bool> SetNewGroupAsync(GroupDto newGroup);

    Task<GroupDto?> GetGroupInfoInCacheAsync(Guid groupId);

    Task<bool> JoinGroupInCacheAsync(GroupDto group, JoinGroupRequestDto reuqester);

    Task<bool> LeaveGroupInCacheAsync(GroupDto group, DefaultGroupRequestDto requester);

    Task FlushGroupKeysAsync();

    #endregion Group Methods
}