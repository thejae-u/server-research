using WebServer.Dtos;

namespace WebServer.Services;

public interface IGroupService
{
    Task<UserGroupDto> CreateNewGroupAsync(Guid userId);

    Task<UserGroupDto?> GetGroupInfoAsync(Guid groupId);

    Task<UserGroupDto?> JoinGroupAsync(Guid groupId, Guid userId);
}