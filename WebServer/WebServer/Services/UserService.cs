using AutoMapper;
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
    private readonly IMapper _mapper;

    public UserService(GameServerDbContext gdbContext, IConfiguration configuration, IMapper mapper)
    {
        _gdbContext = gdbContext;
        _configuration = configuration;
        _mapper = mapper;
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

        // UserDto로 매핑하여 반환
        return _mapper.Map<UserDto>(user);
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

        var userDto = _mapper.Map<UserDto>(user);

        return new UserResponseDto
        {
            Token = tokenString,
            User = userDto
        };
    }

    public async Task<UserDto?> GetUSerByIdAsync(Guid id)
    {
        var user = await _gdbContext.Users.FindAsync(id);
        return _mapper.Map<UserDto>(user);
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
        return _mapper.Map<IEnumerable<UserDto>>(users);
    }
}