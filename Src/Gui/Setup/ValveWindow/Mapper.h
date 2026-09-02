#pragma once

struct ValveInfo;

class ValveWindow;

class Mapper
{
public:
    static ValveInfo read(const ValveWindow& view);

    static void write(ValveWindow& view,
                      const ValveInfo& model);
};