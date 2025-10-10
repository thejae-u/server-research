using WebServer.Dtos;

namespace WebServer.Services;

public interface ICachingService
{
    #region General Methods

    /// <summary>
    /// Redis에 저장 된 Key의 값을 가져옵니다.
    /// </summary>
    Task<T?> GetAsync<T>(string key);

    /// <summary>
    /// Key에 대한 Value를 Redis에 저장합니다.
    /// </summary>
    Task SetAsync<T>(string key, T value, TimeSpan? expiry = null);

    /// <summary>
    /// Redis에 저장 된 Key를 제거합니다.
    /// </summary>
    Task<bool> RemoveAsync(string key);

    #endregion General Methods

    #region User Methods

    /// <summary>
    /// 유저의 로그인 상태를 활성화 합니다.
    /// </summary>
    Task<bool> SetUserLoginStatusAsync(Guid userId, TimeSpan? expiry = null);

    /// <summary>
    /// 유저의 로그인 상태를 반환합니다.
    /// </summary>
    Task<bool> IsUserLoggedInAsync(Guid userId);

    /// <summary>
    /// 유저의 로그아웃 상태를 활성화 합니다. (캐시 삭제)
    /// </summary>
    Task<bool> ClearUserLoginStatusAsync(Guid userId);

    #endregion User Methods

    #region Group Methods

    /// <summary>
    /// Guid를 통해 그룹에 대한 키를 얻습니다.
    /// </summary>
    string GetGroupKey(Guid groupId);

    /// <summary>
    /// 저장 된 모든 그룹에 대해 정보를 얻습니다.
    /// </summary>
    Task<IEnumerable<GroupDto>> GetAllGroupInCacheAsync();

    /// <summary>
    /// 새로운 그룹에 대해서 Redis에 저장합니다.
    /// </summary>
    Task<bool> SetNewGroupAsync(GroupDto newGroup);

    /// <summary>
    /// 특정 그룹에 대해 Guid를 통해 정보를 얻습니다.
    /// </summary>
    Task<GroupDto?> GetGroupInfoInCacheAsync(Guid groupId);

    /// <summary>
    /// 특정 그룹에 대해 유저를 추가합니다.
    /// </summary>
    Task<bool> JoinGroupInCacheAsync(GroupDto group, JoinGroupRequestDto reuqester);

    /// <summary>
    /// 특정 그룹에 대해 유저를 제거합니다.
    /// </summary>
    Task<bool> LeaveGroupInCacheAsync(GroupDto group, DefaultGroupRequestDto requester);

    /// <summary>
    /// 관리자용 Method로 모든 그룹에 대한 캐시를 삭제합니다.
    /// </summary>
    Task FlushGroupKeysAsync();

    #endregion Group Methods
}