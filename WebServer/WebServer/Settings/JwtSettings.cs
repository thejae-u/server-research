using System.ComponentModel.DataAnnotations;

namespace WebServer.Settings;

public class JwtSettings
{
    [Required]
    public string AccessKey { get; set; } = string.Empty;

    [Required]
    public string RefreshKey { get; set; } = string.Empty;

    public string Issuer { get; set; } = string.Empty;
    public string Audience { get; set; } = string.Empty;

    public static bool Validate(JwtSettings? settings)
    {
        return settings != null
            && !string.IsNullOrEmpty(settings.AccessKey)
            && !string.IsNullOrEmpty(settings.Issuer)
            && !string.IsNullOrEmpty(settings.Audience)
            && !string.IsNullOrEmpty(settings.RefreshKey);
    }
}