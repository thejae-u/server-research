using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.EntityFrameworkCore;
using Microsoft.IdentityModel.Tokens;
using Microsoft.OpenApi.Models;
using StackExchange.Redis;
using System.Text;
using WebServer.Data;
using WebServer.Services;
using WebServer.Settings;

namespace WebServer
{
    public class Program
    {
        public static void Main(string[] args)
        {
            var AllowSpecificOrigins = "_allowSpecificOrigins";
            var builder = WebApplication.CreateBuilder(args);

            // Add services to the container.

            builder.Services.AddControllers();
            builder.Services.AddEndpointsApiExplorer();
            builder.Services.AddSwaggerGen(options =>
            {
                options.SwaggerDoc("v1", new OpenApiInfo
                {
                    Title = "Game Server API",
                    Version = "v1",
                    Description = "Lockstep Web Server API Document",
                    Contact = new OpenApiContact
                    {
                        Name = "Kim Jae Woo",
                        Email = "contact@thejaeu.com"
                    }
                });
            });

            // Authorize service
            builder.Services.AddAuthentication(options =>
            {
                options.DefaultAuthenticateScheme = JwtBearerDefaults.AuthenticationScheme;
                options.DefaultChallengeScheme = JwtBearerDefaults.AuthenticationScheme;
            }).AddJwtBearer(options =>
            {
                var jwtSettings = builder.Configuration.GetSection("Jwt").Get<JwtSettings>()
                      ?? throw new InvalidOperationException("JwtSettings is not configured.");

                options.TokenValidationParameters = new TokenValidationParameters
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
            });

            builder.Services.AddAuthorization();

            // JWT Settings 등록
            builder.Services.Configure<JwtSettings>(builder.Configuration.GetSection("Jwt"));

            // DI
            builder.Services.AddSingleton<ITokenService, TokenService>();
            builder.Services.AddScoped<IUserService, UserService>();
            builder.Services.AddScoped<IGroupService, GroupService>();

            // DB Context
            var connectionString = builder.Configuration.GetConnectionString("PersistantDB");
            builder.Services.AddDbContext<GameServerDbContext>(options =>
            {
                options.UseNpgsql(connectionString);
            });

            // Distributed Cache
            var redisConnectionString = builder.Configuration.GetConnectionString("Redis") ?? throw new ArgumentException("Failed to read RedisCache Connection String.");
            builder.Services.AddSingleton<IConnectionMultiplexer>(ConnectionMultiplexer.Connect(redisConnectionString));
            builder.Services.AddStackExchangeRedisCache(options =>
            {
                options.Configuration = builder.Configuration.GetConnectionString("Redis");
                options.InstanceName = "RedisCache";
            });

            // SignalR
            builder.Services.AddSignalR();

            // docker container 보안 완화
            builder.Services.AddCors(options =>
            {
                options.AddPolicy(name: AllowSpecificOrigins,
                    policy =>
                    {
                        policy.WithOrigins("http://localhost", "null").AllowAnyHeader().AllowAnyMethod().AllowCredentials();
                    });
            });

            var app = builder.Build();

            // Internal Error Handler
            app.UseExceptionHandler(appError =>
            {
                appError.Run(async context =>
                {
                    context.Response.StatusCode = 500;
                    context.Response.ContentType = "application/json";

                    await context.Response.WriteAsJsonAsync(new
                    {
                        context.Response.StatusCode,
                        Message = "서버 내부 오류가 발생했습니다. 관리자에게 문의하세요."
                    });
                });
            });

            // Configure the HTTP request pipeline.
            if (app.Environment.IsDevelopment())
            {
                app.UseSwagger();
                app.UseSwaggerUI(options =>
                {
                    options.SwaggerEndpoint("/swagger/v1/swagger.json", "Game Server API");
                    options.RoutePrefix = string.Empty;
                });
            }

            app.UseCors(AllowSpecificOrigins);

            app.UseHttpsRedirection();

            app.UseAuthentication();

            app.UseAuthorization();

            app.MapControllers();

            app.Run();
        }
    }
}