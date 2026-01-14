A full-stack banking simulation project featuring a secure REST API backend and a modern React frontend.

Tech Stack:
Backend: C++ 20, Crow (REST API), libpqxx (PostgreSQL), jwt-cpp, cpr, nlohmann/json.
Frontend: React, Vite, Framer Motion.
Database: PostgreSQL.

1. User Registration
Secure account creation. Passwords are hashed (using salt) before being stored in the database to ensure security.

![register](https://github.com/user-attachments/assets/02ca6c49-3f43-4f11-982c-57ed60f315cb)

2. Authorization (Login)
The system uses JWT (JSON Web Tokens) for secure authentication. Upon login, the server issues a token that grants access to protected resources.

![login](https://github.com/user-attachments/assets/33bdb93e-1cca-4a36-bb81-04cfd09b6f75)

3. Money Transfer
Core banking functionality. Users can transfer funds to other clients. The backend ensures data integrity and checks balances using atomic database transactions.

![make transaction](https://github.com/user-attachments/assets/178ef27b-9aa2-4566-b94c-ac9239b00c68)

4. Creating a Savings Jar
Users can create multiple "Jars" (savings goals) with specific targets and images to organize their finances.

![create jar](https://github.com/user-attachments/assets/9f5d8fcf-6e86-4c11-93e9-58d38651f351)

5. Deposit to Jar
Users can move funds from their main wallet to a specific savings jar. The progress bar updates dynamically.

![deposit jar](https://github.com/user-attachments/assets/55428db4-ec94-4ba3-b6c4-606e54434a66)

6. Withdraw from Jar
Flexible savings management allows withdrawing money back to the main balance without closing the jar.

![withdraw jar](https://github.com/user-attachments/assets/aa2e1f86-0605-4312-a953-178381b1535f)

7. Break Jar
If the goal is reached or cancelled, the jar can be broken. The entire accumulated amount is instantly returned to the main balance, and the jar is deleted.

![breake jar](https://github.com/user-attachments/assets/40d648c8-c2d6-4dd0-8bcf-bcf1fd785812)

8. Transaction History
All transfers are logged in a dedicated history table. Users can view their incoming and outgoing transactions with timestamps.

![show history](https://github.com/user-attachments/assets/f0426f04-884c-4217-b900-84c6b57a0f0c)

9. Admin Tools Panel
Users with admin rights have access to a special dashboard for user management and moderation.

![show admintools](https://github.com/user-attachments/assets/f72208ca-a876-46d9-a2e0-b8b389ff6d53)

10. Banning a User
Admins can restrict access for specific users by providing a ban reason. The status is updated via a PATCH request.

![ban user](https://github.com/user-attachments/assets/11265130-4b70-4b3a-8db6-80198fb5a79a)

11. Transaction Attempt (Banned User)
Blocked users are restricted from performing any financial operations. The server rejects the transfer request with a 403 Forbidden status.

![try make transactions](https://github.com/user-attachments/assets/8c8c79e4-5aaf-430d-a414-cd3b247bbcbd)

12. Creating Jar Attempt (Banned User)
Restricted users cannot create new assets or savings goals while the ban is active.

![try make jar](https://github.com/user-attachments/assets/a7ff77fb-c2b8-4131-aa11-7931ad9eb370)

13. Unbanning a User
Admins can restore user access. Once unbanned, the user immediately regains the ability to use banking features.

![unban](https://github.com/user-attachments/assets/9bfa007b-9b06-4068-8881-f6c9034e54e0)

15. Logout
Secure session termination. The client removes the JWT token and redirects the user to the landing page.

![unlogin](https://github.com/user-attachments/assets/00f03a6f-6243-48ff-b2f0-c3ac108df5dd)
