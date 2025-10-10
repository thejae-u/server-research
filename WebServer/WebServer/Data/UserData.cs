using System.ComponentModel.DataAnnotations;

namespace WebServer.Data;

public static class RoleCaching
{
    public const string Player = "Player";
    public const string Admin = "Admin";
    public const string Internal = "Internal";
}

public class UserData
{
    [Key]
    public required Guid Uid { get; set; }

    [Required]
    public required string Username { get; set; }

    [Required]
    public required string PasswordHash { get; set; }

    public DateTime CreatedAt { get; set; }

    public string Role { get; set; } = RoleCaching.Player;
}