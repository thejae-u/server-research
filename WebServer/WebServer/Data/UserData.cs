using System.ComponentModel.DataAnnotations;

namespace WebServer.Data;

public class UserData
{
    [Key]
    public required Guid Uid { get; set; }

    [Required]
    public required string Username { get; set; }

    [Required]
    public required string PasswordHash { get; set; }

    public DateTime CreatedAt { get; set; }

    public string Role { get; set; } = "Player";
}