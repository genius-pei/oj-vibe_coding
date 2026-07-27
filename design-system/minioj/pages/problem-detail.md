# Problem Detail Page Overrides

> **PROJECT:** MiniOJ
> **Generated:** 2026-07-27 11:46:15
> **Page Type:** Product Detail

> ⚠️ **IMPORTANT:** Rules in this file **override** the Master file (`design-system/MASTER.md`).
> Only deviations from the Master are documented here. For all other rules, refer to the Master.

---

## Page-Specific Rules

### Layout Overrides

- **Max Width:** 1400px or full-width
- **Grid:** 12-column grid for data flexibility
- **Sections:** 1. Intro (Vertical), 2. The Journey (Horizontal Track), 3. Detail Reveal, 4. Vertical Footer

### Spacing Overrides

- **Content Density:** High — optimize for information display

### Typography Overrides

- No overrides — use Master typography

### Color Overrides

- **Strategy:** Continuous palette transition. Chapter colors. Progress bar #000000.

### Component Overrides

- Avoid: Single large bundle
- Avoid: Single error message at top of form
- Avoid: No feedback after submit

---

## Page-Specific Components

- No unique components for this page

---

## Recommendations

- Effects: Card hover effects (lift/scale), icon animations on scroll, feature toggle animations, smooth section transitions
- Performance: Split code by route/feature
- Forms: Show error below related input
- Forms: Show loading then success/error state
- CTA Placement: Floating Sticky CTA or End of Horizontal Track
