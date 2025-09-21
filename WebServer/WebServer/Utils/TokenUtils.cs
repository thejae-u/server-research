using Microsoft.IdentityModel.Tokens;
using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;
using System.Text;
using WebServer.Data;
using WebServer.Dtos;
using WebServer.Settings;

namespace WebServer.Utils;

public static class TokenUtils
{
    public static JwtSettings GetJwtSettings(this IConfiguration config)
    {
        var jwtSettings = config.GetSection("Jwt").Get<JwtSettings>();
        if (jwtSettings == null
            || string.IsNullOrEmpty(jwtSettings.Issuer)
            || string.IsNullOrEmpty(jwtSettings.Audience)
            || string.IsNullOrEmpty(jwtSettings.Key))
        {
            throw new ArgumentNullException(nameof(jwtSettings));
        }

        return jwtSettings;
    }

    public static TokenValidationParameters GetTokenValidationParam(this JwtSettings jwtSettings)
    {
        var validateionParmeters = new TokenValidationParameters
        {
            ValidateIssuer = true,
            ValidIssuer = jwtSettings.Issuer,

            ValidateAudience = true,
            ValidAudience = jwtSettings.Audience,

            ValidateLifetime = true,
            ValidateIssuerSigningKey = true,
            IssuerSigningKey = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(jwtSettings.Key)),

            ClockSkew = TimeSpan.Zero
        };

        return validateionParmeters;
    }

    public static string GenerateToken(this JwtSettings jwtSettings, UserData user, double expireTime = 1)
    {
        var claims = new[]
        {
            new Claim(JwtRegisteredClaimNames.Sub, user.UID.ToString()),
            new Claim(JwtRegisteredClaimNames.Jti, Guid.NewGuid().ToString()),
            new Claim(ClaimTypes.Role, user.Role)
        };

        var key = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(jwtSettings.Key));
        var creds = new SigningCredentials(key, SecurityAlgorithms.HmacSha256);
        var expires = DateTime.UtcNow.AddHours(expireTime);

        var token = new JwtSecurityToken(jwtSettings.Issuer, jwtSettings.Audience, claims, expires: expires, signingCredentials: creds);
        var tokenString = new JwtSecurityTokenHandler().WriteToken(token);

        return tokenString;
    }
}

public class TokenValidateRequest
{
    public required string Token { get; set; }
}