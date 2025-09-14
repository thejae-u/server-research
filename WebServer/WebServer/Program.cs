using AutoMapper;
using Microsoft.EntityFrameworkCore;
using Microsoft.OpenApi.Models;
using StackExchange.Redis;
using WebServer.Controllers;
using WebServer.Data;
using WebServer.Services;

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

            // DI
            builder.Services.AddScoped<IUserService, UserService>();
            builder.Services.AddScoped<IGroupService, GroupService>();

            // Auto Mapper 설정
            builder.Services.AddAutoMapper(cfg =>
            {
                cfg.AddMaps(typeof(Program).Assembly);
            });

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

            app.UseAuthorization();

            app.MapControllers();

            app.Run();
        }
    }
}