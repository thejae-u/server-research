using WebServer.Dtos;

namespace WebServer.Services;

public interface IUserService
{
    // 회원가입에 성공하면 UserDto 반환, 실패 시 null 반환
    Task<UserDto?> RegisterAsync(UserRegisterDto userRegisterDto);

    // 로그인 성공 시 Token과 UserDto 반환, 실패 시 null 반환
    Task<UserResponseDto?> LoginAsync(UserLoginDto userLoginDto);

    Task<bool> LogoutAsync(UserSimpleDto userLogoutDto);

    // 리프레시 토큰으로 액세스 토큰 발급
    Task<UserResponseDto?> RefreshAsync(string refreshToken);

    // Id 조회 성공 시 UserDto 반환, 실패 시 null 반환
    Task<UserDto?> GetUSerByIdAsync(Guid id);

    Task<bool> DeleteUserAsync(UserDeleteDto userDeleteDto);

    Task<IEnumerable<UserDto>> GetAllUserAsync();
}