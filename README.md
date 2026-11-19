Minimal Hostinger-ready PHP site

Place the contents of the public_html/ directory into your Hostinger account's public_html folder (or upload this repository directly if supported). This small site includes:

- index.php - landing page
- synchrotron.php - game page with canvas scaffold
- inc/header.php, inc/footer.php - shared templates
- assets/css/style.css - minimal stylesheet
- assets/js/synchrotron.js - placeholder game scaffold

Deployment notes:
- Hostinger supports PHP; ensure the PHP version is 8.0+ for best compatibility.
- Upload files preserving the public_html structure.
- If using a local dev server, run: php -S localhost:8000 -t public_html

Customization:
- Swap the placeholder canvas game code in assets/js/synchrotron.js with your Synchrotron implementation.
- Edit inc/header.php and inc/footer.php to add site-wide meta tags or navigation.
- Add assets (images, icons) to public_html/assets/ and reference them by path.

License: MIT (you can change it as you like)
