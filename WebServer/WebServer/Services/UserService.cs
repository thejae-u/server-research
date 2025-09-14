using Microsoft.EntityFrameworkCore;
using System.IdentityModel.Tokens.Jwt;
using Microsoft.IdentityModel.Tokens;

using System.Diagnostics;
using System.Security.Claims;
using System.Text;

using WebServer.Data;
using WebServer.Dtos;
using WebServer.Settings;

namespace WebServer.Services;

public class UserService : IUserService
{
    private readonly GameServerDbContext _gdbContext;
    private readonly IConfiguration _configuration;

    public UserService(GameServerDbContext gdbContext, IConfiguration configuration)
    {
        _gdbContext = gdbContext;
        _configuration = configuration;
    }

    public async Task<UserDto?> RegisterAsync(UserRegisterDto userRegisterDto)
    {
        if (await _gdbContext.Users.AnyAsync(u => u.Username == userRegisterDto.Username))
            return null; // Already Exsist

        var user = new UserData
        {
            UID = Guid.NewGuid(),
            Username = userRegisterDto.Username,
            PasswordHash = BCrypt.Net.BCrypt.HashPassword(userRegisterDto.Password),
            Role = RoleCaching.Player,
            CreatedAt = DateTime.UtcNow
        };

        // DB 반영
        await _gdbContext.AddAsync(user);
        await _gdbContext.SaveChangesAsync();

        // UserDto로 직접 매핑하여 반환
        return new UserDto().Mapping(user);
    }

    public async Task<UserResponseDto?> LoginAsync(UserLoginDto userLoginDto)
    {
        var user = await _gdbContext.Users.FirstOrDefaultAsync(u => u.Username == userLoginDto.Username);

        if (user == null || !BCrypt.Net.BCrypt.Verify(userLoginDto.Password, user.PasswordHash))
        {
            return null; // 사용자가 없거나 비밀번호가 틀림
        }

        var jwtSettings = _configuration.GetSection("Jwt").Get<JwtSettings>();
        if (JwtSettings.Validate(jwtSettings))
            throw new ArgumentNullException(nameof(jwtSettings));

        Debug.Assert(jwtSettings is not null);

        var claims = new[]
        {
            new Claim(JwtRegisteredClaimNames.Sub, user.UID.ToString()),
            new Claim(JwtRegisteredClaimNames.Jti, Guid.NewGuid().ToString()),
            new Claim(ClaimTypes.Role, user.Role)
        };

        var key = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(jwtSettings.Key));
        var creds = new SigningCredentials(key, SecurityAlgorithms.HmacSha256);
        var expires = DateTime.UtcNow.AddHours(1);

        var token = new JwtSecurityToken(jwtSettings.Issuer, jwtSettings.Audience, claims, expires: expires, signingCredentials: creds);
        var tokenString = new JwtSecurityTokenHandler().WriteToken(token);

        var userDto = new UserDto().Mapping(user);
        return new UserResponseDto
        {
            Token = tokenString,
            User = userDto
        };
    }

    public async Task<UserDto?> GetUSerByIdAsync(Guid id)
    {
        var user = await _gdbContext.Users.FindAsync(id);
        var userDto = user == null ? null : new UserDto().Mapping(user);
        return userDto;
    }

    public async Task<bool> DeleteUserAsync(UserDeleteDto userDeleteDto)
    {
        var user = await _gdbContext.Users.FirstOrDefaultAsync(u => u.UID == userDeleteDto.UID);
        if (user == null || !BCrypt.Net.BCrypt.Verify(userDeleteDto.Password, user.PasswordHash))
            return false;

        _gdbContext.Users.Remove(user);
        await _gdbContext.SaveChangesAsync();

        return true;
    }

    public async Task<IEnumerable<UserDto>> GetAllUserAsync()
    {
        var users = await _gdbContext.Users.ToListAsync();

        var userDtoList = new List<UserDto>();
        foreach (var user in users)
        {
            userDtoList.Add(new UserDto().Mapping(user));
        }

        return userDtoList;
    }
}