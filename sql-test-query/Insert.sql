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