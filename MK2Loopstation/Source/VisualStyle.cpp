#include "VisualStyle.h"
#include <array>

void VisualStyle::prepareToggle(juce::Button& button)
{
    button.setClickingTogglesState(true);
    button.setWantsKeyboardFocus(false);
    button.setMouseClickGrabsKeyboardFocus(false);
    button.setTitle(button.getName());
}

void VisualStyle::ledStroke(juce::Graphics& g, const juce::Path& path, float width,
                            juce::Colour colour, bool lit)
{
    const auto bounds = path.getBounds();
    if (lit)
    {
        for (int layer = 4; layer >= 1; --layer)
        {
            g.setColour(colour.withAlpha(0.025f + (4 - layer) * 0.022f));
            g.strokePath(path, juce::PathStrokeType(width + layer * 2.0f));
        }
    }
    const auto base = lit ? colour : juce::Colour(0xff42474e);
    juce::ColourGradient diffuser(base.interpolatedWith(juce::Colours::white, lit ? 0.5f : 0.12f),
                                 bounds.getTopLeft(), base.darker(0.2f), bounds.getBottomRight(), false);
    diffuser.addColour(0.3, base);
    diffuser.addColour(0.63, base.brighter(lit ? 0.18f : 0.0f));
    g.setGradientFill(diffuser);
    g.strokePath(path, juce::PathStrokeType(width));
    g.setColour(lit ? colour.interpolatedWith(juce::Colours::white, 0.75f).withAlpha(0.65f)
                    : juce::Colour(0xff555b63));
    g.strokePath(path, juce::PathStrokeType(width * 0.22f));
}

void VisualStyle::grain(juce::Graphics& g, juce::Rectangle<float> bounds, float opacity)
{
    // Fixed pattern: no random changes or animation during repaint.
    g.setColour(juce::Colours::white.withAlpha(opacity));
    for (int y = static_cast<int>(bounds.getY()); y < bounds.getBottom(); y += 3)
        for (int x = static_cast<int>(bounds.getX()) + (y % 7); x < bounds.getRight(); x += 7)
            g.fillRect(static_cast<float>(x), static_cast<float>(y), 0.65f, 0.45f);
}

void VisualStyle::trackNumber(juce::Graphics& g, int number, juce::Rectangle<float> bounds)
{
    // Custom wide, squared numerals traced from the reference proportions.
    // No installed font dependency or substitute system glyphs.
    juce::Path path;
    const auto polygon = [&path](std::initializer_list<juce::Point<float>> points)
    {
        auto it = points.begin();
        path.startNewSubPath(*it++);
        for (; it != points.end(); ++it) path.lineTo(*it);
        path.closeSubPath();
    };
    switch (number)
    {
        case 1: polygon({ {2,0},{12,0},{12,17},{7,17},{7,4},{2,4} }); break;
        case 2: polygon({ {0,0},{21,0},{24,3},{24,7},{21,10},{5,10},{5,13},
                          {24,13},{24,17},{0,17},{0,9},{3,6},{19,6},{19,4},{0,4} }); break;
        case 3: polygon({ {0,0},{21,0},{24,3},{24,14},{21,17},{0,17},{0,13},
                          {19,13},{19,10},{4,10},{4,6},{19,6},{19,4},{0,4} }); break;
        case 4: polygon({ {5,0},{10,0},{5,8},{17,8},{17,0},{22,0},{22,17},
                          {17,17},{17,12},{0,12},{0,8} }); break;
        case 5: polygon({ {0,0},{24,0},{24,4},{5,4},{5,6},{21,6},{24,9},
                          {24,14},{21,17},{0,17},{0,13},{19,13},{19,10},{0,10} }); break;
        default: return;
    }
    path.applyTransform(juce::AffineTransform::scale(bounds.getWidth() / 24.0f, bounds.getHeight() / 17.0f)
                            .translated(bounds.getX(), bounds.getY()));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff0f2f4), bounds.getTopLeft(),
                                         juce::Colour(0xffaeb7c4), bounds.getBottomLeft(), false));
    g.fillPath(path);
}

void VisualStyle::lcdText(juce::Graphics& g, const juce::String& text,
                          juce::Rectangle<float> bounds, float pixel)
{
    const auto glyph = [](juce::juce_wchar c) -> std::array<int, 7>
    {
        switch (c)
        {
            case '0': return {14,17,17,17,17,17,14};
            case '1': return {4,12,4,4,4,4,14};
            case '2': return {14,17,1,2,4,8,31};
            case '3': return {30,1,1,14,1,1,30};
            case '4': return {2,6,10,18,31,2,2};
            case '5': return {31,16,16,30,1,1,30};
            case '6': return {14,16,16,30,17,17,14};
            case '7': return {31,1,2,4,8,8,8};
            case '8': return {14,17,17,14,17,17,14};
            case '9': return {14,17,17,15,1,1,14};
            case '.': return {0,0,0,0,0,12,12};
            case 'M': return {17,27,21,21,17,17,17};
            case 'E': return {31,16,16,30,16,16,31};
            case 'O': return {14,17,17,17,17,17,14};
            case 'R': return {30,17,17,30,20,18,17};
            case 'Y': return {17,17,10,4,4,4,4};
            case 'F': return {31,16,16,30,16,16,16};
            case 'X': return {17,17,10,4,10,17,17};
            case 'V': return {17,17,17,17,17,10,4};
            case 'B': return {30,17,17,30,17,17,30};
            case 'A': return {14,17,17,31,17,17,17};
            case 'C': return {14,17,16,16,16,17,14};
            case 'D': return {30,17,17,17,17,17,30};
            case 'G': return {14,17,16,23,17,17,15};
            case 'H': return {17,17,17,31,17,17,17};
            case 'I': return {14,4,4,4,4,4,14};
            case 'K': return {17,18,20,24,20,18,17};
            case 'L': return {16,16,16,16,16,16,31};
            case 'N': return {17,25,25,21,19,19,17};
            case 'P': return {30,17,17,30,16,16,16};
            case 'Q': return {14,17,17,17,21,18,13};
            case 'W': return {17,17,17,21,21,21,10};
            case '>': return {16,8,4,2,4,8,16};
            case 'S': return {15,16,16,14,1,1,30};
            case 'T': return {31,4,4,4,4,4,4};
            case 'U': return {17,17,17,17,17,17,14};
            case '#': return {10,31,10,10,31,10,0};
            case '+': return {0,4,4,31,4,4,0};
            case '-': return {0,0,0,31,0,0,0};
            case ':': return {0,4,4,0,4,4,0};
            case 'e': return {0,0,14,17,31,16,14};
            case 'm': return {0,0,26,21,21,21,21};
            case 'o': return {0,0,14,17,17,17,14};
            case 'r': return {0,0,22,25,16,16,16};
            case 'y': return {0,0,17,17,15,1,14};
            default: return {};
        }
    };
    float x = bounds.getCentreX() - (text.length() * 6.0f - 1.0f) * pixel * 0.5f;
    const float y = bounds.getCentreY() - 7.0f * pixel * 0.5f;
    for (auto character : text)
    {
        const auto rows = glyph(character);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if ((rows[static_cast<size_t>(row)] & (1 << (4 - col))) != 0)
                    g.fillRect(x + col * pixel, y + row * pixel, pixel, pixel);
        x += 6.0f * pixel;
    }
}
