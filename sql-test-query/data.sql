drop database mmo_server_data;

create database mmo_server_data;

use mmo_server_data;

CREATE TABLE users (
    uuid INT PRIMARY KEY AUTO_INCREMENT NOT NULL,
    user_name VARCHAR(10) NOT NULL UNIQUE,
    user_password VARCHAR(64) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE user_log (
    uuid INT,
    log_text TEXT,
    FOREIGN KEY (uuid) REFERENCES users(uuid) ON DELETE CASCADE
);

CREATE TABLE server_log (
    log_text TEXT
);

INSERT INTO users (user_name, user_password, created_at) VALUES
('alice', SHA2('password1', 256), NOW()),
('bob', SHA2('password2', 256), NOW()),
('charlie', SHA2('password3', 256), NOW()),
('david', SHA2('password4', 256), NOW()),
('eve', SHA2('password5', 256), NOW()),
('frank', SHA2('password6', 256), NOW()),
('grace', SHA2('password7', 256), NOW()),
('heidi', SHA2('password8', 256), NOW()),
('ivan', SHA2('password9', 256), NOW()),
('judy', SHA2('password10', 256), NOW());

select * from users;


desc users;