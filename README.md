Introduction
====

All-Ears Mail is an email server focused on user privacy and security.

All message data is encrypted using the user's public key. Not even the receiver address is stored in plaintext. The encrypted data is simply associated with the user's public key.

Even addresses registered by users are considered private. They are stored as [Argon2](https://en.wikipedia.org/wiki/Argon2) hashes.

Traditional protocols such as POP and IMAP are not supported. SMTP is only supported for receiving email from other servers.

There are no usernames, and no "logging in". [Authenticated Encryption](https://en.wikipedia.org/wiki/Authenticated_encryption) is used to prove ownership of the secret key, while the public key acts as the user's identifier.

All-Ears comes with a simple, security-focused web server built in. There is a basic web interface utilizing an open web API.

This software is experimental and largely untested. While every attempt is made to ensure All-Ears is safe to use, its security is unproven.

Address Types & Storage
====

All-Ears provides two types of addresses:
* Normal addresses, which are 1-24 characters (0-9, a-z, dot, hyphen)
* Shield addresses, which are 36 hexadecimal characters (0-9, a-f)

Shield addresses are random, and are generated by the server. They are intended for temporary use, or for extra security (see below).

All addresses are 18 bytes. A six-bit encoding is applied to normal addresses, allowing for 24 characters.

All addresses are stored in a binary format. Addresses technically act as both types, but the alias is not intended to be used.

Addresses are 144 bits, but are stored as 64-bit Argon2 hashes. This means there may be multiple unintentional aliases for addresses, although they're very unlikely to be encountered normally.

Argon2 hashes take time and power to calculate, but easily guessable addresses are likely to be discovered easily. Shield addresses consist of 144 random bits, making them resistant against such attacks.

Message Storage
====

Messages are stored in libsodium [Sealed Boxes](https://download.libsodium.org/doc/public-key_cryptography/sealed_boxes).

Messages consist of two Sealed Boxes:
* HeadBox, which contains header data, generated by the server
* BodyBox, which contains the message body, generated by either the client or the server

Messages exist in four types:
* IntMsg (Internal Message), mail sent to another user on the service
* ExtMsg (External Message), email
* Text Notes created by the user
* Files uploaded by the user (planned)

It is impossible to tell which type a message is without decryption using the user's secret key.

Messages are padded to the nearest 1024 bytes, to obfuscate their size.

Email is compressed with Brotli prior to encryption.

All messages are stored in an Sqlite3 database.

Database Format
====

All-Ears stores all user data in two Sqlite3 databases. Both are configured to use Secure Delete, meaning any deleted data is overwritten.

Note that Sealed Boxes can only be opened with the user's secret key.

**Users.aed** contains three tables:

`userdata`:
* `upk64` (integer): The first 64 bits of the user's public key (required to be unique for each user)
* `publickey` (blob): The user's full public key
* `level` (integer): The user's level, limiting their capabilities
* `notedata` (blob): A Sealed Box containing data such as contacts (not Text Notes despite the name)
* `addrdata` (blob): A Sealed Box containing the address data for the user
* `gkdata` (blob): A Sealed Box containing the Gatekeeper rules for the user

`address`:
* `hash` (integer): An Argon2 hash of the address
* `upk64` (integer): The first 64 bits of the user's public key
* `flags` (integer): Settings associated with the address (should it receive email, should Gatekeeper be used, etc)

`gatekeeper`:
* `hash` (integer): A Blake2 hash of the Gatekeeper rule
* `upk64` (integer): The first 64 bits of the user's public key

**Messages.aed** contains one table:

`msg`:
* `upk64` (integer): The first 64 bits of the user's public key
* `msg` (blob): HeadBox followed by BodyBox (Sealed Boxes containing header and body data, respectively)

Gatekeeper (anti-spam)
====

All-Ears allows all messages through by default, even obvious spam. Gatekeeper can be enabled on an address-by-address basis to filter out email based on configurable rules.

Dependencies
====

Data storage is handled with [Sqlite](https://sqlite.org).

TLS is provided by [MbedTLS](https://tls.mbed.org).

Cryptography is done using [libsodium](https://libsodium.org).

Compression is provided by [Brotli](https://github.com/google/brotli).

Email senders are geolocated thanks to [MaxMind](https://dev.maxmind.com/geoip/geoip2/downloadable/).
