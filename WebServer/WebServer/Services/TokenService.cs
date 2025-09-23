using Microsoft.IdentityModel.Tokens;
using Microsoft.Extensions.Options;
using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;
using System.Text;
using WebServer.Data;
using WebServer.Settings;

namespace WebServer.Services;

public class TokenService : ITokenService
{
    private readonly JwtSettings _jwtSettings;

    public TokenService(IOptions<JwtSettings> jwtSettingsOPtions)
    {
        _jwtSettings = jwtSettingsOPtions.Value;
    }

    public string GenerateAccessToken(UserData user, double expireTime = 1)
    {
        return Generate(user, _jwtSettings.AccessKey, expireTime);
    }

    public string GenerateRefreshToken(UserData user, double expireTime = 30)
    {
        return Generate(user, _jwtSettings.RefreshKey, expireTime);
    }

    private string Generate(UserData user, string jwtKey, double expireTime)
    {
        var claims = new[]
        {
            new Claim(JwtRegisteredClaimNames.Sub, user.UID.ToString()),
            new Claim(JwtRegisteredClaimNames.Jti, Guid.NewGuid().ToString()),
            new Claim(ClaimTypes.Role, user.Role)
        };

        var key = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(jwtKey));
        var creds = new SigningCredentials(key, SecurityAlgorithms.HmacSha256);

        DateTime expires;
        if (jwtKey == _jwtSettings.RefreshKey)
            expires = DateTime.UtcNow.AddDays(expireTime); // Refresh Token has Long Term
        else
            expires = DateTime.UtcNow.AddHours(expireTime); // Access Token has Short Term

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
}

public class TokenValidateRequest
{
    public required string Token { get; set; }
}