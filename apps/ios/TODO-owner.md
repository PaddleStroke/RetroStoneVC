# iOS: the owner's to-do

1. Apple Developer Program (organisation, D-U-N-S): developer.apple.com/programs, 99 USD/year.
2. Identifiers: App IDs `com.eightbcraft.retrostoneboard`, `com.eightbcraft.retrostonearcade`.
3. Certificate: one Apple Distribution (.p12, no Mac needed: README "Making the certificate").
4. Profiles: one App Store profile per App ID.
5. App Store Connect: the two apps; in each, a Non-Consumable in-app purchase:
   `com.eightbcraft.retrostoneboard.unlock`, `com.eightbcraft.retrostonearcade.unlock` (price, names).
6. Agreements: Paid Apps agreement, bank, tax.
7. API key: Users and Access > Integrations, role App Manager; download the .p8.
8. GitHub secrets (README table): `APPLE_TEAM_ID`, `IOS_DIST_CERT_P12_BASE64`, `IOS_DIST_CERT_PASSWORD`,
   `IOS_BOARD_PROFILE_BASE64`, `IOS_ARCADE_PROFILE_BASE64`, `ASC_KEY_ID`, `ASC_ISSUER_ID`, `ASC_KEY_P8`.
9. Push (or Actions > iOS > Run workflow): builds land in TestFlight.
10. TestFlight: add internal testers (your Apple Account), install on a phone, buy the unlock (sandbox: free).
11. Store listing per app: name, subtitle, description, keywords, privacy policy URL, support URL,
    age rating, "Data Not Collected".
12. Screenshots: 6.9" iPhone and 13" iPad, from TestFlight builds.
13. Submit the version with its in-app purchase for review.
