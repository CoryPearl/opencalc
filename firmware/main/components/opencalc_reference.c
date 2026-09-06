#include "opencalc_reference.h"

#define E(symbol, name, mass, group, period, category) \
    {symbol, name, mass, group, period, category}

static const opencalc_element_t ELEMENTS[118] = {
    E("H", "Hydrogen", "1.008", 1, 1, OPENCALC_ELEMENT_NONMETAL),
    E("He", "Helium", "4.0026", 18, 1, OPENCALC_ELEMENT_NOBLE_GAS),
    E("Li", "Lithium", "6.94", 1, 2, OPENCALC_ELEMENT_ALKALI),
    E("Be", "Beryllium", "9.0122", 2, 2, OPENCALC_ELEMENT_ALKALINE),
    E("B", "Boron", "10.81", 13, 2, OPENCALC_ELEMENT_METALLOID),
    E("C", "Carbon", "12.011", 14, 2, OPENCALC_ELEMENT_NONMETAL),
    E("N", "Nitrogen", "14.007", 15, 2, OPENCALC_ELEMENT_NONMETAL),
    E("O", "Oxygen", "15.999", 16, 2, OPENCALC_ELEMENT_NONMETAL),
    E("F", "Fluorine", "18.998", 17, 2, OPENCALC_ELEMENT_HALOGEN),
    E("Ne", "Neon", "20.180", 18, 2, OPENCALC_ELEMENT_NOBLE_GAS),
    E("Na", "Sodium", "22.990", 1, 3, OPENCALC_ELEMENT_ALKALI),
    E("Mg", "Magnesium", "24.305", 2, 3, OPENCALC_ELEMENT_ALKALINE),
    E("Al", "Aluminum", "26.982", 13, 3, OPENCALC_ELEMENT_POST_TRANSITION),
    E("Si", "Silicon", "28.085", 14, 3, OPENCALC_ELEMENT_METALLOID),
    E("P", "Phosphorus", "30.974", 15, 3, OPENCALC_ELEMENT_NONMETAL),
    E("S", "Sulfur", "32.06", 16, 3, OPENCALC_ELEMENT_NONMETAL),
    E("Cl", "Chlorine", "35.45", 17, 3, OPENCALC_ELEMENT_HALOGEN),
    E("Ar", "Argon", "39.948", 18, 3, OPENCALC_ELEMENT_NOBLE_GAS),
    E("K", "Potassium", "39.098", 1, 4, OPENCALC_ELEMENT_ALKALI),
    E("Ca", "Calcium", "40.078", 2, 4, OPENCALC_ELEMENT_ALKALINE),
    E("Sc", "Scandium", "44.956", 3, 4, OPENCALC_ELEMENT_TRANSITION),
    E("Ti", "Titanium", "47.867", 4, 4, OPENCALC_ELEMENT_TRANSITION),
    E("V", "Vanadium", "50.942", 5, 4, OPENCALC_ELEMENT_TRANSITION),
    E("Cr", "Chromium", "51.996", 6, 4, OPENCALC_ELEMENT_TRANSITION),
    E("Mn", "Manganese", "54.938", 7, 4, OPENCALC_ELEMENT_TRANSITION),
    E("Fe", "Iron", "55.845", 8, 4, OPENCALC_ELEMENT_TRANSITION),
    E("Co", "Cobalt", "58.933", 9, 4, OPENCALC_ELEMENT_TRANSITION),
    E("Ni", "Nickel", "58.693", 10, 4, OPENCALC_ELEMENT_TRANSITION),
    E("Cu", "Copper", "63.546", 11, 4, OPENCALC_ELEMENT_TRANSITION),
    E("Zn", "Zinc", "65.38", 12, 4, OPENCALC_ELEMENT_TRANSITION),
    E("Ga", "Gallium", "69.723", 13, 4, OPENCALC_ELEMENT_POST_TRANSITION),
    E("Ge", "Germanium", "72.630", 14, 4, OPENCALC_ELEMENT_METALLOID),
    E("As", "Arsenic", "74.922", 15, 4, OPENCALC_ELEMENT_METALLOID),
    E("Se", "Selenium", "78.971", 16, 4, OPENCALC_ELEMENT_NONMETAL),
    E("Br", "Bromine", "79.904", 17, 4, OPENCALC_ELEMENT_HALOGEN),
    E("Kr", "Krypton", "83.798", 18, 4, OPENCALC_ELEMENT_NOBLE_GAS),
    E("Rb", "Rubidium", "85.468", 1, 5, OPENCALC_ELEMENT_ALKALI),
    E("Sr", "Strontium", "87.62", 2, 5, OPENCALC_ELEMENT_ALKALINE),
    E("Y", "Yttrium", "88.906", 3, 5, OPENCALC_ELEMENT_TRANSITION),
    E("Zr", "Zirconium", "91.224", 4, 5, OPENCALC_ELEMENT_TRANSITION),
    E("Nb", "Niobium", "92.906", 5, 5, OPENCALC_ELEMENT_TRANSITION),
    E("Mo", "Molybdenum", "95.95", 6, 5, OPENCALC_ELEMENT_TRANSITION),
    E("Tc", "Technetium", "[98]", 7, 5, OPENCALC_ELEMENT_TRANSITION),
    E("Ru", "Ruthenium", "101.07", 8, 5, OPENCALC_ELEMENT_TRANSITION),
    E("Rh", "Rhodium", "102.91", 9, 5, OPENCALC_ELEMENT_TRANSITION),
    E("Pd", "Palladium", "106.42", 10, 5, OPENCALC_ELEMENT_TRANSITION),
    E("Ag", "Silver", "107.87", 11, 5, OPENCALC_ELEMENT_TRANSITION),
    E("Cd", "Cadmium", "112.41", 12, 5, OPENCALC_ELEMENT_TRANSITION),
    E("In", "Indium", "114.82", 13, 5, OPENCALC_ELEMENT_POST_TRANSITION),
    E("Sn", "Tin", "118.71", 14, 5, OPENCALC_ELEMENT_POST_TRANSITION),
    E("Sb", "Antimony", "121.76", 15, 5, OPENCALC_ELEMENT_METALLOID),
    E("Te", "Tellurium", "127.60", 16, 5, OPENCALC_ELEMENT_METALLOID),
    E("I", "Iodine", "126.90", 17, 5, OPENCALC_ELEMENT_HALOGEN),
    E("Xe", "Xenon", "131.29", 18, 5, OPENCALC_ELEMENT_NOBLE_GAS),
    E("Cs", "Cesium", "132.91", 1, 6, OPENCALC_ELEMENT_ALKALI),
    E("Ba", "Barium", "137.33", 2, 6, OPENCALC_ELEMENT_ALKALINE),
    E("La", "Lanthanum", "138.91", 3, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Ce", "Cerium", "140.12", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Pr", "Praseodymium", "140.91", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Nd", "Neodymium", "144.24", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Pm", "Promethium", "[145]", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Sm", "Samarium", "150.36", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Eu", "Europium", "151.96", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Gd", "Gadolinium", "157.25", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Tb", "Terbium", "158.93", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Dy", "Dysprosium", "162.50", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Ho", "Holmium", "164.93", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Er", "Erbium", "167.26", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Tm", "Thulium", "168.93", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Yb", "Ytterbium", "173.05", 0, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Lu", "Lutetium", "174.97", 3, 6, OPENCALC_ELEMENT_LANTHANIDE),
    E("Hf", "Hafnium", "178.49", 4, 6, OPENCALC_ELEMENT_TRANSITION),
    E("Ta", "Tantalum", "180.95", 5, 6, OPENCALC_ELEMENT_TRANSITION),
    E("W", "Tungsten", "183.84", 6, 6, OPENCALC_ELEMENT_TRANSITION),
    E("Re", "Rhenium", "186.21", 7, 6, OPENCALC_ELEMENT_TRANSITION),
    E("Os", "Osmium", "190.23", 8, 6, OPENCALC_ELEMENT_TRANSITION),
    E("Ir", "Iridium", "192.22", 9, 6, OPENCALC_ELEMENT_TRANSITION),
    E("Pt", "Platinum", "195.08", 10, 6, OPENCALC_ELEMENT_TRANSITION),
    E("Au", "Gold", "196.97", 11, 6, OPENCALC_ELEMENT_TRANSITION),
    E("Hg", "Mercury", "200.59", 12, 6, OPENCALC_ELEMENT_TRANSITION),
    E("Tl", "Thallium", "204.38", 13, 6, OPENCALC_ELEMENT_POST_TRANSITION),
    E("Pb", "Lead", "207.2", 14, 6, OPENCALC_ELEMENT_POST_TRANSITION),
    E("Bi", "Bismuth", "208.98", 15, 6, OPENCALC_ELEMENT_POST_TRANSITION),
    E("Po", "Polonium", "[209]", 16, 6, OPENCALC_ELEMENT_POST_TRANSITION),
    E("At", "Astatine", "[210]", 17, 6, OPENCALC_ELEMENT_HALOGEN),
    E("Rn", "Radon", "[222]", 18, 6, OPENCALC_ELEMENT_NOBLE_GAS),
    E("Fr", "Francium", "[223]", 1, 7, OPENCALC_ELEMENT_ALKALI),
    E("Ra", "Radium", "[226]", 2, 7, OPENCALC_ELEMENT_ALKALINE),
    E("Ac", "Actinium", "[227]", 3, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Th", "Thorium", "232.04", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Pa", "Protactinium", "231.04", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("U", "Uranium", "238.03", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Np", "Neptunium", "[237]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Pu", "Plutonium", "[244]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Am", "Americium", "[243]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Cm", "Curium", "[247]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Bk", "Berkelium", "[247]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Cf", "Californium", "[251]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Es", "Einsteinium", "[252]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Fm", "Fermium", "[257]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Md", "Mendelevium", "[258]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("No", "Nobelium", "[259]", 0, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Lr", "Lawrencium", "[266]", 3, 7, OPENCALC_ELEMENT_ACTINIDE),
    E("Rf", "Rutherfordium", "[267]", 4, 7, OPENCALC_ELEMENT_TRANSITION),
    E("Db", "Dubnium", "[268]", 5, 7, OPENCALC_ELEMENT_TRANSITION),
    E("Sg", "Seaborgium", "[269]", 6, 7, OPENCALC_ELEMENT_TRANSITION),
    E("Bh", "Bohrium", "[270]", 7, 7, OPENCALC_ELEMENT_TRANSITION),
    E("Hs", "Hassium", "[269]", 8, 7, OPENCALC_ELEMENT_TRANSITION),
    E("Mt", "Meitnerium", "[278]", 9, 7, OPENCALC_ELEMENT_UNKNOWN),
    E("Ds", "Darmstadtium", "[281]", 10, 7, OPENCALC_ELEMENT_UNKNOWN),
    E("Rg", "Roentgenium", "[282]", 11, 7, OPENCALC_ELEMENT_UNKNOWN),
    E("Cn", "Copernicium", "[285]", 12, 7, OPENCALC_ELEMENT_UNKNOWN),
    E("Nh", "Nihonium", "[286]", 13, 7, OPENCALC_ELEMENT_UNKNOWN),
    E("Fl", "Flerovium", "[289]", 14, 7, OPENCALC_ELEMENT_UNKNOWN),
    E("Mc", "Moscovium", "[290]", 15, 7, OPENCALC_ELEMENT_UNKNOWN),
    E("Lv", "Livermorium", "[293]", 16, 7, OPENCALC_ELEMENT_UNKNOWN),
    E("Ts", "Tennessine", "[294]", 17, 7, OPENCALC_ELEMENT_HALOGEN),
    E("Og", "Oganesson", "[294]", 18, 7, OPENCALC_ELEMENT_NOBLE_GAS),
};

static const uint8_t PERIODIC_LAYOUT[9][18] = {
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {3,4,0,0,0,0,0,0,0,0,0,0,5,6,7,8,9,10},
    {11,12,0,0,0,0,0,0,0,0,0,0,13,14,15,16,17,18},
    {19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36},
    {37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54},
    {55,56,0,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86},
    {87,88,0,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118},
    {0,0,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,0},
    {0,0,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,0},
};

static const opencalc_reference_entry_t MATH_REFERENCES[] = {
    {"Quadratic formula", "x=(-b +/- sqrt(b^2-4ac))/(2a)", "x", "Roots of ax^2+bx+c=0"},
    {"Distance 2D", "d=sqrt((x2-x1)^2+(y2-y1)^2)", "coordinate units", "Euclidean distance"},
    {"Circle", "A=pi*r^2   C=2*pi*r", "area, length", "Area and circumference"},
    {"Triangle", "A=b*h/2", "area", "Base perpendicular to height"},
    {"Pythagorean", "a^2+b^2=c^2", "length", "Right triangles"},
    {"Slope", "m=(y2-y1)/(x2-x1)", "unitless", "Line rate of change"},
    {"Arithmetic series", "S=n*(a1+an)/2", "same as terms", "Finite arithmetic sequence"},
    {"Geometric series", "S=a1*(1-r^n)/(1-r)", "same as terms", "Finite, r != 1"},
    {"Compound interest", "A=P*(1+r/n)^(n*t)", "currency", "n compounds per year"},
    {"Radians", "radians=degrees*pi/180", "rad", "Angle conversion"},
};

static const opencalc_reference_entry_t PHYSICS_REFERENCES[] = {
    {"Newton second law", "F=m*a", "N = kg*m/s^2", "Net force"},
    {"Kinetic energy", "KE=m*v^2/2", "J", "Translational motion"},
    {"Potential energy", "PE=m*g*h", "J", "Near Earth's surface"},
    {"Momentum", "p=m*v", "kg*m/s", "Linear momentum"},
    {"Ohm law", "V=I*R", "V, A, ohm", "DC resistance"},
    {"Electric power", "P=V*I=I^2*R=V^2/R", "W", "Electrical power"},
    {"Wave speed", "v=f*lambda", "m/s", "Frequency times wavelength"},
    {"Photon energy", "E=h*f", "J", "h=6.62607015e-34 J*s"},
    {"Coulomb force", "F=k*q1*q2/r^2", "N", "k=8.9875517923e9"},
    {"Ideal gas", "P*V=n*R*T", "Pa*m^3", "R=8.314462618 J/(mol*K)"},
};

static const opencalc_reference_entry_t ENGINEERING_REFERENCES[] = {
    {"Series resistors", "Req=R1+R2+...", "ohm", "Same current through each"},
    {"Parallel resistors", "1/Req=1/R1+1/R2+...", "ohm", "Same voltage across each"},
    {"Voltage divider", "Vout=Vin*R2/(R1+R2)", "V", "Unloaded divider"},
    {"RC time constant", "tau=R*C", "s", "63.2 percent charge time"},
    {"LED resistor", "R=(Vs-Vf)/I", "ohm", "Choose nearest safe standard value"},
    {"ADC voltage", "Vin=code*Vref/(2^N-1)", "V", "Ideal N-bit converter"},
    {"Power in resistor", "P=I^2*R=V^2/R", "W", "Check resistor rating"},
    {"Beam stress", "sigma=M*y/I", "Pa", "Elastic bending stress"},
    {"Thermal rise", "deltaT=P*theta", "deg C", "theta in deg C/W"},
    {"Gear ratio", "ratio=teeth_out/teeth_in", "unitless", "Torque scales by ratio"},
    {"RPM to rad/s", "omega=RPM*2*pi/60", "rad/s", "Rotational speed"},
    {"Decibels power", "dB=10*log10(P2/P1)", "dB", "Use 20 log for amplitude"},
};

const opencalc_element_t *opencalc_element_get(int atomic_number)
{
    return atomic_number >= 1 && atomic_number <= 118 ? &ELEMENTS[atomic_number - 1] : NULL;
}

const char *opencalc_element_category_name(opencalc_element_category_t category)
{
    static const char *const names[] = {
        "nonmetal", "noble gas", "alkali metal", "alkaline earth", "metalloid",
        "halogen", "post-transition", "transition metal", "lanthanide", "actinide", "unknown"
    };
    return category >= 0 && category <= OPENCALC_ELEMENT_UNKNOWN ? names[category] : "unknown";
}

int opencalc_periodic_atomic_number_at(int row, int column)
{
    return row >= 0 && row < 9 && column >= 0 && column < 18 ? PERIODIC_LAYOUT[row][column] : 0;
}

int opencalc_periodic_find_position(int atomic_number, int *row, int *column)
{
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 18; ++x) {
            if (PERIODIC_LAYOUT[y][x] == atomic_number) {
                if (row != NULL) *row = y;
                if (column != NULL) *column = x;
                return 1;
            }
        }
    }
    return 0;
}

const char *opencalc_reference_category_name(opencalc_reference_category_t category)
{
    static const char *const names[] = {"Math", "Physics", "Engineering"};
    return category >= 0 && category < OPENCALC_REFERENCE_CATEGORY_COUNT ? names[category] : "Reference";
}

size_t opencalc_reference_count(opencalc_reference_category_t category)
{
    switch (category) {
    case OPENCALC_REFERENCE_MATH: return sizeof(MATH_REFERENCES) / sizeof(MATH_REFERENCES[0]);
    case OPENCALC_REFERENCE_PHYSICS: return sizeof(PHYSICS_REFERENCES) / sizeof(PHYSICS_REFERENCES[0]);
    case OPENCALC_REFERENCE_ENGINEERING: return sizeof(ENGINEERING_REFERENCES) / sizeof(ENGINEERING_REFERENCES[0]);
    default: return 0;
    }
}

const opencalc_reference_entry_t *opencalc_reference_get(opencalc_reference_category_t category,
                                                          size_t index)
{
    if (index >= opencalc_reference_count(category)) return NULL;
    switch (category) {
    case OPENCALC_REFERENCE_MATH: return &MATH_REFERENCES[index];
    case OPENCALC_REFERENCE_PHYSICS: return &PHYSICS_REFERENCES[index];
    case OPENCALC_REFERENCE_ENGINEERING: return &ENGINEERING_REFERENCES[index];
    default: return NULL;
    }
}

