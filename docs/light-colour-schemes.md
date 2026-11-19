# Light colour schemes

Display → Colour scheme now has Light and Dark tabs. The Light tab offers the
original Light scheme plus Cloud / blue, Sand / bronze and Rose / plum. All
three additions use pale surfaces, dark readable text and distinct accents;
warnings retain their amber/green/red meanings. New light schemes use a darker
fault red for readable text on pale surfaces.

The selected light scheme is saved independently in `light_scheme`. Existing
preferences without this key retain the original Light palette; invalid IDs
fall back to it. Existing dark indices remain unchanged. The assignable Night
mode action switches between the last chosen light and dark palettes. Scheme
selection does not alter RF, arm requests, models or telemetry settings.

There are four options per tab so every choice and the saved-selection marker
fit the landscape screen. Portrait layout is also covered by simulation. The
host test checks text, muted labels, accent text, header labels and fault-text
contrast for the new palettes. The focused UI audit exercises real picker
callbacks, persistence, Night toggle, missing/corrupt preferences, tab and row
bounds, and screenshots in both orientations.

Build/tests run on GitHub. Deployment details will be recorded here after the
user-authorized upload.
