CREATE DATABASE IF NOT EXISTS minioj
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_0900_ai_ci;

USE minioj;

CREATE TABLE IF NOT EXISTS problems (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    title VARCHAR(255) NOT NULL,
    description_md TEXT NOT NULL,
    difficulty ENUM('easy', 'medium', 'hard') NOT NULL,
    time_limit_ms INT UNSIGNED NOT NULL DEFAULT 500,
    memory_limit_mb INT UNSIGNED NOT NULL DEFAULT 256,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    CONSTRAINT chk_problems_time_limit CHECK (time_limit_ms > 0),
    CONSTRAINT chk_problems_memory_limit CHECK (memory_limit_mb > 0)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS testcases (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    problem_id INT UNSIGNED NOT NULL,
    input TEXT NOT NULL,
    expected_output TEXT NOT NULL,
    is_sample BOOLEAN NOT NULL DEFAULT FALSE,
    score INT UNSIGNED NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    INDEX idx_testcases_problem_id_id (problem_id, id),
    CONSTRAINT fk_testcases_problem
        FOREIGN KEY (problem_id) REFERENCES problems (id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS tags (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    name VARCHAR(64) NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_tags_name (name)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS problem_tags (
    problem_id INT UNSIGNED NOT NULL,
    tag_id INT UNSIGNED NOT NULL,
    PRIMARY KEY (problem_id, tag_id),
    INDEX idx_problem_tags_tag_id (tag_id, problem_id),
    CONSTRAINT fk_problem_tags_problem
        FOREIGN KEY (problem_id) REFERENCES problems (id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,
    CONSTRAINT fk_problem_tags_tag
        FOREIGN KEY (tag_id) REFERENCES tags (id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS users (
    id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(20) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('admin', 'user') NOT NULL DEFAULT 'user',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_users_username (username)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS sessions (
    id CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    user_id INT UNSIGNED NOT NULL,
    expires_at DATETIME NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    INDEX idx_sessions_user_id (user_id),
    INDEX idx_sessions_expires_at (expires_at),
    CONSTRAINT fk_sessions_user
        FOREIGN KEY (user_id) REFERENCES users (id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
) ENGINE=InnoDB;
