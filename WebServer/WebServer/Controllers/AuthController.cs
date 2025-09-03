using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using WebServer.Data;

namespace WebServer.Controllers;

[ApiController]
[Route("api/auth")]
public class AuthController : ControllerBase
{
    private readonly GameServerDbContext _context;

    public AuthController(GameServerDbContext context)
    {
        _context = context;
    }

    [HttpGet]
    public async Task<IActionResult> GetAllPlayer()
    {
        var users = await _context.Users.ToListAsync();
        return users.Count != 0 ? Ok(users) : NoContent();
    }
}