using System.ComponentModel.DataAnnotations;

namespace WebServer.Dtos;

public class UserGroupDto
{
    [Required]
    public required Guid GroupId { get; set; }

    public required IEnumerable<Guid> Players { get; set; }

    public required DateTime CreatedAt { get; set; }

    // 그룹에 대한 평균 rtt와 평균 오차율
    public double AverageRtt { get; set; }

    public double AverageErrorRate { get; set; }
}