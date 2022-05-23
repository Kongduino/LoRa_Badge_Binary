# BADGE PROJECT

## Initialization

Let's separate the setup of the badges from their main app. Make an app to program the EEPROM from a RAK4631. Accept commands from Serial and read / write data from the RAK4631 to the EEPROM.

```
/name xxxxxxxx	--> Initialize with a name xxxxxxx and a new UUID
/uuid?			--> read back name & UUID
/read			--> read back name & UUID
```

## Badge Message Format

Binary, LPP style

### Contents

* Message UUID  (4 bytes)
* From ID (1 byte) – Fewer senders
* To ID (2 bytes) - 65535 possible recipients (0 = *)
* Resend Counter (1 byte) – Limit resends to X (3?) resends. If counter = limit, do not resend further.
* Message Type (1 byte) – Canned message (0x00) or free-style (byte = length).
* Message contents (1 byte..XX bytes) 1 byte for canned.

A pre-canned message will be 4+1+2+1+1+1, ie ten bytes. THIS can be sent at any SF/BW combination...

| UUID | From | To | Resend | Type | Contents |
| :----: | :----: | :----: | :----: | :----: | :----:|
| 00.01.02.03 | 0F | AB.CD | 00 | 00 | 00 |

### Canned Messages

* 00		Call your office/service
* 01		(Followed by 5 bytes) Call this number
  * Example:

| UUID | From | To | Resend | Type | Contents | Extra |
| :----: | :----: | :----: | :----: | :----: | :----:| :----:|
| 00.01.02.03 | 0F | AB.CD | 00 | 00 | 01 | 06.30.59.1e.4b |
|   |   |   |  |  |  | 06.4889.3075 |

  Two phone digits are encoded in one byte. 5 bytes needed for a full phone number.
* 02	Come back to office

etc, etc... Yours to decide.

Make a JSON file that will be loaded by the Xojo app, and a `.h` file that will be loaded by the Arduino app. OR, maybe, write them to the EEPROM too.
