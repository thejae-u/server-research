using Microsoft.IdentityModel.Tokens;
using Microsoft.Extensions.Options;
using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;
using System.Text;
using WebServer.Data;
using WebServer.Settings;
using StackExchange.Redis;
using System.Security.Cryptography;
using WebServer.Dtos;

namespace WebServer.Services;

public class TokenService : ITokenService
{
    private const string refreshKeyPrefix = "user:";
    private const string refreshKeySuffix = ":refreshtoken";

    private readonly JwtSettings _jwtSettings;
    private readonly IConnectionMultiplexer _redis;

    public TokenService(IOptions<JwtSettings> jwtSettingsOPtions, IConnectionMultiplexer redis)
    {
        _jwtSettings = jwtSettingsOPtions.Value;
        _redis = redis;
    }

    public string GenerateAccessToken(UserData user, double expireTime = 1)
    {
        var claims = new Claim[]
        {
            new Claim(JwtRegisteredClaimNames.Sub, user.UID.ToString()),
            new Claim(JwtRegisteredClaimNames.Name, user.Username),
            new Claim(JwtRegisteredClaimNames.Jti, Guid.NewGuid().ToString()),
            new Claim(ClaimTypes.Role, user.Role)
        };

        var expires = DateTime.UtcNow.AddHours(expireTime);

        return GenerateToken(claims, _jwtSettings.AccessKey, expires);
    }

    public async Task<string> GenerateRefreshToken(UserData user, double expireTime = 30)
    {
        var claims = new Claim[]
        {
            new(JwtRegisteredClaimNames.Sub, user.UID.ToString()),
            new(JwtRegisteredClaimNames.Jti, Guid.NewGuid().ToString())
        };

        var expires = DateTime.UtcNow.AddDays(expireTime);
        var refreshToken = GenerateToken(claims, _jwtSettings.RefreshKey, expires);

        var db = _redis.GetDatabase();
        var redisKey = $"{refreshKeyPrefix}{user.UID}{refreshKeySuffix}";
        var hashBytes = SHA256.HashData(Encoding.UTF8.GetBytes(refreshToken));
        var hashedToken = Convert.ToBase64String(hashBytes);

        var expiryTimeSpan = expires - DateTime.UtcNow;

        await db.StringSetAsync(redisKey, hashedToken, expiryTimeSpan);

        return refreshToken;
    }

    private string GenerateToken(Claim[] claims, string jwtKey, DateTime expires)
    {
        var key = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(jwtKey));
        var creds = new SigningCredentials(key, SecurityAlgorithms.HmacSha256);

        var token = new JwtSecurityToken(_jwtSettings.Issuer, _jwtSettings.Audience, claims, expires: expires, signingCredentials: creds);
        var tokenString = new JwtSecurityTokenHandler().WriteToken(token);

        return tokenString;
    }

    public TokenValidationParameters GetTokenValidationParam()
    {
        var validateionParmeters = new TokenValidationParameters
        {
            ValidateIssuer = true,
            ValidIssuer = _jwtSettings.Issuer,

            ValidateAudience = true,
            ValidAudience = _jwtSettings.Audience,

            ValidateLifetime = true,
            ValidateIssuerSigningKey = true,
            IssuerSigningKey = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(_jwtSettings.AccessKey)),

            ClockSkew = TimeSpan.Zero
        };

        return validateionParmeters;
    }

    public async Task<UserDtoForTokenResponse?> ValidateRefreshToken(string refreshToken)
    {
        var principal = GetPrincipalByToken(refreshToken, _jwtSettings.RefreshKey);
        if (principal is null) return null;

        var userIdString = principal.FindFirstValue(ClaimTypes.NameIdentifier);
        if (!Guid.TryParse(userIdString, out var userId)) return null;

        var db = _redis.GetDatabase();
        var redisKey = $"{refreshKeyPrefix}{userId}{refreshKeySuffix}";
        var storedHashedToken = await db.StringGetAsync(redisKey);

        if (storedHashedToken.IsNullOrEmpty) return null;

        var hashBytes = SHA256.HashData(Encoding.UTF8.GetBytes(refreshToken));
        var hashedToken = Convert.ToBase64String(hashBytes);

        if (storedHashedToken != hashedToken) return null;

        return new UserDtoForTokenResponse()
        {
            UserId = userId,
            Principal = principal
        };
    }

    private ClaimsPrincipal? GetPrincipalByToken(string token, string key)
    {
        try
        {
            var tokenValidationParameters = new TokenValidationParameters
            {
                ValidateIssuer = true,
                ValidIssuer = _jwtSettings.Issuer,
                ValidateAudience = true,
                ValidAudience = _jwtSettings.Audience,
                ValidateLifetime = true, // 만료 시간 검증
                IssuerSigningKey = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(key)),
                ValidateIssuerSigningKey = true,
                ClockSkew = TimeSpan.Zero
            };

            var tokenHandler = new JwtSecurityTokenHandler();
            var principal = tokenHandler.ValidateToken(token, tokenValidationParameters, out SecurityToken securityToken);

            if (securityToken is not JwtSecurityToken jwtSecurityToken ||
                !jwtSecurityToken.Header.Alg.Equals(SecurityAlgorithms.HmacSha256, StringComparison.InvariantCultureIgnoreCase))
            {
                throw new SecurityTokenException("Invalid token");
            }

            return principal;
        }
        catch
        {
            return null; // 검증 실패 시 null 반환
        }
    }
}

public class TokenValidateRequest
{
    public required string Token { get; set; }
}