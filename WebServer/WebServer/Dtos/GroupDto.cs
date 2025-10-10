using System.ComponentModel.DataAnnotations;

namespace WebServer.Dtos;

public class GroupDto
{
    [Required]
    public required Guid GroupId { get; set; }

    [Required]
    public required string Name { get; set; }

    [Required]
    public required UserSimpleDto Owner { get; set; }

    [Required]
    public required List<UserSimpleDto> Players { get; set; }

    public DateTime CreatedAt { get; set; }
}

public class CreateGroupRequestDto
{
    [Required]
    [StringLength(20, MinimumLength = 3)]
    public required string GroupName { get; set; }

    [Required]
    public required UserSimpleDto Requester { get; set; }
}

public class JoinGroupRequestDto
{
    [Required]
    public required Guid GroupId { get; set; }

    [Required]
    public required UserSimpleDto Requester { get; set; }
}

public class DefaultGroupRequestDto
{
    [Required]
    public required Guid GroupId { get; set; }

    [Required]
    public required Guid UserId { get; set; }
}