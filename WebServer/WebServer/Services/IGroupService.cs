using WebServer.Dtos;
using WebServer.Utils;

namespace WebServer.Services;

public interface IGroupService
{
    Task<IEnumerable<GroupDto>> GetAllGroupAsync();

    Task<GroupDto?> CreateNewGroupAsync(CreateGroupRequestDto ownerUser);

    Task<Result<GroupDto?>> GetGroupInfoAsync(Guid groupId);

    Task<Result<GroupDto?>> JoinGroupAsync(JoinGroupRequestDto requestDto);

    Task<Result<GroupDto?>> LeaveGroupAsync(DefaultGroupRequestDto requestDto);

    // Admin Method Area
    Task<bool> FlushGroupsAsync(UserDto requester);

    Task<bool> FlushGroupAsyncTest();
}