#pragma once

void setup_connection();

void listenhttp();

void ShowMessageBox(const std::string& title, const std::string& message);

void sendWebhookMessage(const std::string& webhook_url, const std::string& content);