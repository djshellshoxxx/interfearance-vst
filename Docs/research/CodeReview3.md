# Code review 3: auditor's UI work in 0c81eb3

Reviewer: read-only review agent, 2026-09-24.

**Scope.** `git show -w 0c81eb3 -- Source/UI Source/Plugin/FactoryPresets.cpp`: Widgets, SlotPanel, StudioPanel /
EnvironmentPanel, MainView, Illustrations, LaneEditor, HelpContent, and the Environment presets. I also looked at
the GUI snapshots (`main_1000x700`, `main_default`, `tab_lane`, `tab_studio`, `tab_create`, `tab_routing`).
`tab_environment.png` does not exist yet, so the Environment tab layout below is worked out from the code.

**Uncommitted work at review time.** `SlotPanel.*` and `StudioPanel.*` have edits that are not committed yet:
- the Why-quiet box now uses a `ScopedMessageBox`, which fixes the old m3 pattern there;
- an RF Transmitter choice was added, and the Environment row now has 10 columns.

Findings are against the commit unless marked **(working tree)**.

Severity: **Blocker** = won't compile / will crash / fails a test. **Major** = reachable defect (layout that
hides controls, broken gesture, wrong text). **Minor** = everything else.

---

## 0. Blockers

None. No new code path allocates or locks on the audio thread, and nothing in the UI calls a DSP setter. The new
readouts only load telemetry atomics (`stageIn/Out`, `groundLoopDb`, `excitationLimited`,
`excitationEffectiveDb`, `fbEffective`, `ppq`, `laneStep`) or parameter values through `getRawParameterValue`.

## Checked and correct

- **Lifetimes.**
  - `ContextPanel` stops its timer and closes the open host gesture (`endGuide()`) in its destructor, so closing
    the editor mid-ramp leaves no gesture open.
  - `MainView::messageBox` is the last member, so it is destroyed first and closes its box before the views go.
  - The Why-quiet callback uses a `SafePointer`, and in the working tree it is a scoped box.
  - The new tab and badge have no timers of their own.
- **Parameter gestures.** The guided ramp is one begin / N × `setValueNotifyingHost` / end. Illustrations skip
  `beginEdit` / `endEdit` for the whole right-click cycle, and Gap edits go through the undoable `edit()`.
- **Order toggle.** It is a `ParamToggle` on `swap_order` (a TextButton with a ButtonAttachment); right-click still
  opens the MIDI-learn menu. The → glyph renders (see the snapshots).
- **Program numbers.** The Environment presets are appended at the end of the bank. "Jack Pull Pop" only gains a
  lane step. The Environment category has 4 presets, so `factoryBankIsCompleteAndValid` (≥ 3 each) still passes.
- **MIDI notes in the preset text.** "note 38" (slot A pattern gesture 2 = Pull) and "note 50" (slot B gesture 2)
  match `handleControlEvent`.
- **Help text matches the code** for:
  - the Ceiling (instant attack, stereo-linked, 45 ms release);
  - the smoothing list;
  - Raw Hardware bypassing the hidden extra;
  - the Studio stage descriptions and the stage-meter description;
  - "Limited by loop stability" and "Limited by Natural cap";
  - the badge's "(capped)";
  - the cycle maximum of 4032.

---

## 1. Major findings

### M1. The Details drawer cannot fit at either window size, and "Move toward worn area" pushes it down another 28 px (SlotPanel.cpp:449-523)
**What is wrong.** With a Loose Jack loaded and Details open, `ContextPanel::resized` needs about:

> 20 + 6 (head) + 2×76 (primary knobs) + **28 (new button)** + 6 + ⌈13/4⌉×64 = 256 (5 source + 8 global
> detail knobs) + 42 + 30 (choices / toggles) + 4 + 60 (meters) ≈ **604 px**

The context panel is only about **424 px** tall at 1200×800 and about **360 px** at 1000×700.
`Rectangle::removeFromTop` on an exhausted rectangle returns zero-height cells. So the last row of knobs, the
Source circuit / Mains / Range policy choices, the three toggles and the diagnostic meters get **zero height and
disappear**. They do not overlap anything, which is why `allChildrenInside` passes; the GUI test also never opens
Details. The drawer was already too tall before this commit; the new button made it worse.

The Help text now advertises exactly these drawer controls ("source circuit, input stage, trims, stereo,
environment (buzz, interference), range policy, order and linking, and diagnostic meters"), and so does the new
Why-quiet text in the working tree ("Use Details > Source circuit").

**Fix.**
- Remove the controls that now live elsewhere: Buzz, Interference and Mains are on the Environment tab, and Swap
  order is between the cards.
- Hide the guided-action button while the drawer is open.
- Put the drawer in a `Viewport` so it can never clip.
```cpp
// rebuild(): drop the duplicates now owned by the Environment tab / order toggle
for (int pi : { (int) P::cableLoad, (int) P::inTrim, (int) P::preDrive, (int) P::preHeadroom, (int) P::stereo, (int) P::chaos })
    addChildComponent (detailControls.add (knob (pi)));
addChildComponent (detailControls.add (new ParamChoice (proc, P::inputMode, "Source circuit")));
addChildComponent (detailControls.add (new ParamChoice (proc, P::policy, "Range policy")));
addChildComponent (detailControls.add (new ParamToggle (proc, P::preOn, "Input drive")));
addChildComponent (detailControls.add (new ParamToggle (proc, P::linkGesture, "Link gesture")));

// resized(): the guided action only while the drawer is closed
const bool open = details.getToggleState();
if (moveToWear.isVisible() && ! open)
    moveToWear.setBounds (r.removeFromTop (28).reduced (2).withWidth (std::min (220, r.getWidth() - 4)));
moveToWear.setVisible (type == SourceType::LooseJack && ! open);   // (move the visibility decision here from rebuild)
```
This brings the drawer to about 20+6+152+6+⌈11/4⌉×64 = 192 (+42+30+4+60) ≈ **512 px**, which still does not fit
at 1000×700. So also wrap the drawer content in a `juce::Viewport` (vertical scroll only), or lay the meters out
beside the choices.

Add a GUI test: open Details for each source at 1000×700 and assert that every visible child has height > 0 and
lies inside the panel.

### M2. The guided ramp fights the user and host automation, and can nest host gestures (SlotPanel.cpp:298-334)
**What is wrong.** During the 0.6 s ramp, `timerCallback` writes Move Plug 18 times from its own curve.
- **User drag.** If the user grabs the Move Plug knob, or drags the jack illustration, during the ramp, the
  `SliderAttachment` / `beginEdit` opens a second `beginChangeGesture` on the same parameter. The ramp then keeps
  overwriting the drag, and the host sees begin, begin, end, end.
- **Host automation.** A host in Touch or Latch automation, or playing automation on `jk_insert`, is overwritten
  for 0.6 s.

**Fix.** Abort the ramp as soon as someone else writes the parameter:
```cpp
// members: float guideLast = -1.0f;
void ContextPanel::timerCallback()
{
    auto* p = proc.param (P::jkInsert);
    if (guideLast >= 0.0f && std::abs (p->getValue() - guideLast) > 1.0e-4f)
    {   // the user or the host moved it: hand over, do not fight
        stopTimer(); endGuide(); guideLast = -1.0f; return;
    }
    guidePos = std::min (1.0f, guidePos + 1.0f / 18.0f);
    const float s = guidePos * guidePos * (3.0f - 2.0f * guidePos);
    guideLast = p->convertTo0to1 (guideFrom + (guideTo - guideFrom) * s);
    p->setValueNotifyingHost (guideLast);
    if (guidePos >= 1.0f) { stopTimer(); endGuide(); guideLast = -1.0f; }
}
```
Also skip the ramp while the knob is being dragged (`knob (P::jkInsert)->slider.isMouseButtonDown()`).

### M3. Environment tab: the choice boxes are too narrow to read at 1000×700, and worse in the working tree (StudioPanel.cpp:429-460)
**What is wrong.** The bottom panel is about 706 px wide at 1000×700 and about 906 px at 1200×800.

| Layout | 1000×700 | 1200×800 |
|---|---|---|
| Committed (`cw = w/9`) | 78 px cells, 74 px combo boxes | 100 px cells, 96 px combo boxes |
| Working tree (`cw = w/10`) | 70 px cells, 66 px combo boxes | 90 px cells, 86 px combo boxes |

The choice texts are long: "2-Prong High Power", "Balanced + Input Transformer", "Across Building",
"Heavy Braid", "Check-in". They are truncated to 4-8 characters, so the dropdowns show "Balan…" or "2-Pro…".
Uppercase 10 px captions such as "DIMMER KIND", "BUZZ SOURCE", "TRANSMITTER" and "CABLE SHIELD" need about 70-85 px
and are clipped as well. The knob label "INTERFERENCE" (P::rf) needs about 80 px.

A `ParamChoice` whose current text is unreadable fails the "never colour / shape alone" readability rule, because
the value is not visible.

**Fix.** Give choices 1.6× the width of a knob cell and spread the groups over three rows:
```cpp
// row 1: Mains | Hum | Buzz | Buzz Source | Dimmer | Dimmer Kind
// row 2: Interference | Transmitter || 2G Phone | Distance          (1 px divider between the groups)
// row 3: Other Gear | Outlets | Cable Run | Cable Shield | Interface | readout
const int kw = std::max (64, r.getWidth() / 12);         // knob cell
const int cwChoice = kw * 8 / 5;                         // choice cell (~1.6x)
auto placeRow = [&] (juce::Rectangle<int> row, std::initializer_list<juce::Component*> cs)
{
    for (auto* c : cs)
        place (*c, row.removeFromLeft (dynamic_cast<ParamChoice*> (c) != nullptr ? cwChoice : kw));
};
```
At 208 px of height, three rows of 56 px plus 3 × 14 px headers plus 22 px for the title is 232 px. That does not
fit, so use 48 px rows with the SmallKnob style (36 px knob per theme.md), or keep two rows and let row 2 scroll.
When `tab_environment.png` exists, check it at 1000×700 and 1200×800.

---

## 2. Minor findings

**m1. The excitation badge truncates and can show "Off" (MainView.cpp:490-500, 613).**
- `drawButtonText` uppercases the text and allows 0.7 horizontal scale, on one line. "EXCITATION ON: CALIBRATION
  TONE -12 DB (CAPPED)" needs about 330 px, but the badge gets at most 230 px (190 px at 1000 px), so the text is
  cut.
- While excitation is switched off, `excitationActive` stays true for about 115 ms (the fade), so the badge
  briefly reads "Excitation on: Off …".

Fix:
```cpp
const int et = (int) rawValue (proc, P::excType);
if (excOn && et > 0)
{
    static const char* shortNames[] = { "", "Noise", "Buzz", "Bias", "Tone" };
    juce::String badge = juce::String ("Exc: ") + shortNames[juce::jlimit (0, 4, et)] + " "
                       + juce::String (t.excitationEffectiveDb.load(), 0) + " dB" + (t.excitationLimited.load() ? " cap" : "");
    ...
}
if (excBadge.isVisible() != (excOn && et > 0)) excBadge.setVisible (excOn && et > 0);
```

**m2. A lost mouse-up can leave `popupActive` stuck (Illustrations.cpp:80-94).** If a right-click's mouse-up
never arrives (the menu window grabs the mouse on some platforms or hosts), the next left-drag is ignored, and its
mouse-up skips `endEdit` after `beginEdit` already ran. That leaves the host gesture open. Clear the flag on every
mouse-down:
```cpp
bool Illustration::handlePopup (const juce::MouseEvent& e)
{
    popupActive = e.mods.isPopupMenu();
    if (popupActive) showParamMenu (proc, primaryParam(), *this);
    return popupActive;
}
```

**m3. Unreadable disabled caption (Widgets.cpp:371).** A dormant `ParamChoice` caption is drawn in `Theme::edge`
(#2A303A) on the panel colour (#171B22), a contrast of about 1.3:1. theme.md says inactive text is muted
(#8A929E). The existing disabled `drawButtonText` has the same problem. Use `Theme::textMuted.withAlpha (0.55f)`.
Also, a dormant choice's combo box is disabled, so it cannot be right-clicked for MIDI learn; `ParamKnob` has the
same limitation. Consider keeping the box enabled and greying only the text, as the tooltip already says why.

**m4. Text sizes below theme.md's 11 px label size.**
- "Limited" reason: 9.5 px (Widgets.cpp:230).
- "order" caption: 9 px (MainView.cpp paint).
- Environment sub-headers: 9.5 px.
- Ground-readout sub-line: 10.5 px.

The theme sets labels at 11 px uppercase. At 9-9.5 px, uppercase with tracking, text is hard to read at 100 %
scale. Use 10-11 px, or accept this knowingly and record it in Decisions.md.

**m5. Stage meters miss peaks and don't match the theme (StudioPanel.cpp:24-52, 136).**
- `stageIn/Out` hold the peak of the **last block only**, and `takeStagePeaks` clears them every block. The UI
  reads at 30 Hz, so at 512 samples / 48 kHz it sees one block in three and misses transients.
- theme.md METERS asks for a 1.5 s peak hold and a 20 dB/s fall; this meter falls at 30 dB/s (like LevelMeter).
- At 8 px tall, split 40/60, each bar is 3-5 px.

Fix: have the engine accumulate `max (stored, blockPeak)` and let the UI take it with `exchange (0)`, or keep a
running max in StudioChain. Add a peak-hold line.

**m6. The ground-loop readout is painted text, so screen readers cannot see it (StudioPanel.cpp:419-427).** The
new StageMeters have no accessible title either. Use a `juce::Label` for the readout (it keeps the mono font), and
give `StageMeter` `setTitle (stageNames[s] + " meter")` and `setAccessible (true)`.

**m7. `orderToggle.setTooltip` replaces `paramTooltip` (MainView.cpp:132).** A MIDI mapping of Swap Order is no
longer shown in the tooltip, unlike every other `Param*` control. Append `paramTooltip (proc, P::swapOrder)` to the
custom text, or refresh it from the timer as ParamKnob does.

**m8. The "Move toward worn area" button is 24 px tall (SlotPanel.cpp:471).** theme.md specifies 28 px for
buttons. The button is also used as a status light (its toggle state = "stable here") while still being a
momentary action, which can read as "on". Consider the accent border only, with no filled state.

**m9. Gap on a Dirty Pot is only a gap with the Volume circuit (LaneEditor.cpp:226-265, HelpContent).** "Turn the
pot to minimum" silences the slot only when `pt_circuit` is Volume. With the Tone circuit, minimum is a darker
tone, not a gap. Also, a Gap step keeps pattern gesture index 2 / 0 if the slot's source changes later, and then
plays that source's gesture instead ("Tap" for Cable). Label the menu item accordingly, or make it disabled with a
reason when `pt_circuit == Tone`, and say so in the Help text.

**m10. The cycle readout looks up a parameter by string every refresh (LaneEditor.cpp:410).**
`getRawParameterValue ("lane_div")` does a string-keyed lookup 30 times a second. Use
`rawValue (proc, P::laneDiv)` like the rest of the file. Also guard the modulo (`cycle > 0`). Lengths are clamped
to ≥ 1 in StateIO and the UI, but `setPattern` is public.

**m11. The "Loop Fixed: Isolator" preset text overstates what can be heard (FactoryPresets.cpp).** 2-prong High
leakage through 30 m of drain is about −55 dBFS before the isolator. After 70 dB at mains, and about 45-50 dB at
the 650 Hz harmonics, the "faint high buzz" is around −100 dBFS, which is inaudible. Either use Grounded / Across
Building so the residue is audible, or say "practically silent".

**m12. Help text vs labels.**
- The Help text calls it "Phone Distance", but the knob label is "Distance". Under the "2G PHONE" group header that
  is fine, but the Help could say "Distance (2G phone)".
- Help says "shows a short yellow 'Limited' reason **above** it". That is true only when the knob is not hovered;
  on hover the value readout replaces it (the tooltip keeps it). Add "(or in its tooltip while you adjust it)".

---

## 3. Suggested order

1. M1: fix the drawer overflow (remove duplicates, hide the button when the drawer is open, add a Viewport) and
   add a Details-open GUI test at 1000×700.
2. M3: Environment layout with wider choices. Check `tab_environment.png` at both sizes.
3. M2: abort the ramp when someone else writes the parameter.
4. m1 and m2 (badge text, stale popup flag), then the theme and accessibility items (m3-m8).
5. m9-m12: text and preset wording.
