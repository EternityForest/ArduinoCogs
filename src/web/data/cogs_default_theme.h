// Schema for the wifi settings
static const char nord_theme[] = R"(
/*
Unlike most of Barrel.css, this is NOT public domain, as it uses the Nord color scheme:

MIT License (MIT)

Copyright (c) 2016-present Sven Greb <development@svengreb.de> (https://www.svengreb.de)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

:root {
    --black-1: #23262e;
    --black-2: #3B4252;
    --black-3: #434C5E;
    --black-4: #4C566A;
    --grey-1: #D8DEE9;
    --grey-2: #E5E9F0;
    --grey-3: #ECEFF4;

    --mint: #8FBCBB;
    --ice: #88C0D0;
    --water: #81A1C1;
    --ocean: #5E81AC;

    --red: #BF616A;
    --orange: #D08770;
    --yellow: #EBCB8B;
    --green: #A3BE8C;
    --violet: #B48EAD;

    --teal: var(--ice);
    --blue: var(--ocean);
    --purple: var(--violet);
    --box-bg: white;
    --control-bg: var(--grey-3);
    --alt-control-bg: var(--grey-2);
    --bg: var(--grey-1);
    --fg: var(--black-1);
    --control-fg: var(--black-2);

    --accent-color: var(--ocean);
    --concave-item-box-shadow: none;
    --3d-highlight: var(--control-bg);
    --3d-shadow: var(--control-bg);

    --border-color: var(--grey-1);
    --card-box-shadow: none;
    --window-box-shadow: 2px 2px 6px 0px rgba(10, 33, 83, 0.18);
    --item-box-shadow: 2px 2px 3px 0px rgb(8 35 53 / 12%);
    --table-border-strength: 80%;
}


@media (prefers-color-scheme: dark) {
    :root {
        --bg: var(--black-1);
        --box-bg: var(--black-2);
        --fg: var(--grey-3);
        --control-fg: var(--grey-1);
        --control-bg: var(--black-3);
        --alt-control-bg: var(--black-4);
        --border-color: var(--black-4);
        --window-box-shadow: none;
        --card-box-shadow: none;

    }
}
)";

