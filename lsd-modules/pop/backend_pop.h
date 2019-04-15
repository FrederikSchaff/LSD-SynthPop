/*************************************************************
    backend_pop.h
    Copyright: Frederik Schaff
    Version: 0.11 (jan 2019)

    Licence: MIT


    LSD Population module - backend for LSD (least LSD-GIS)
    written by Frederik Schaff, Ruhr-University Bochum

    for infos on LSD see https://github.com/marcov64/Lsd

    The complete package has the following files:
    [1] readme.md         ; readme file with instructions and information
                           on the underlying model.
    [2] fun_templ_pop.cpp ; a template file for the user model, containing the
                           links to the population backend (which needs to be place in a folder "lsd-modules/pop").
    [3] lsd-modules\pop\backend_pop.h     ; contains the c++ declarations and new macros.
    [4] lsd-modules\pop\backend_pop.cpp   ; contains the c++ core code for the pop backend.
    [5] LICENSE ; the MIT licence
    [6] LSD_macros_pop.html      ; The documentation

    Currently implemented population model:
    BLL : Boucekkine, Raouf; La Croix, David de; Licandro, Omar (2002):
    Vintage Human Capital, Demographic Trends, and Endogenous Growth.
    In Journal of Economic Theory 104 (2), pp. 340–375.
    DOI: 10.1006/jeth.2001.2854.


*************************************************************/

/*  Documentation

    Intention one: Track heritage networks in a population->
    - methods to check for heritage relations (cousin, father, ...) even beyond death

    Implementation: Each agent knows its parent, each parent knows its children.
                    This information needs to stay beyond death.
                    We want to be able to subset by age and gender

    Intention two: Simple statistical model for realisitic (potentially constant) population dynamics.
    - a baseline model that defines death-ages and birth-numbers based on a few properties.

    APIs: Simple access via macros, confirming to LSD standards.
        See the html file.

*/

#include <vector>
#include <string>
#include <tuple>
#include <algorithm>
#include <functional>
#include <map>


class pop_map;
struct pop_person;
struct pop_children; //struct to store and retrieve children.
struct pop_selection; //struct to select some elements.
// struct pop_selection( pop_map *map, char gender, double age_low, double age_high, bool random=true);
struct pop_model_BLL; //specific model
// struct pop_model_BLL(double t_unit, double alpha, double beta);
struct pop_model; //struct to initialise a given survival function.
// struct pop_model(const char model[], double t_unit, double par1, double par2);

struct pop_model_BLL {

    double t_unit;
    double alpha;
    double beta;

    double max_life;    //Maximum possible life. The inverse is equal to the constant pop. birth rate.
    double exp_life;

    //to do: optimise using structs for uncond_sr etc.


    pop_model_BLL(double t_unit, double alpha, double beta)
        : t_unit(t_unit), alpha(alpha), beta(beta)
    {
        if (!(alpha > 1 && beta < 0) ) {
            error_hard( "failure in initialising the BLL population model",
                        "alpha and/or beta outside of specification for humans",
                        "it is required that alpha > 1 and beta < 0" );
        }
        if (!(t_unit > 0) ) {
            error_hard( "failure in initialising the BLL population model",
                        "time unit <= 0",
                        "select an appropriate time unit > 0" );
        }
        max_life =  (- log(alpha) / beta);  //bll (2)
        exp_life =  1.0 / beta + (alpha * log(alpha) ) / ( (1.0 - alpha) * beta); //bll (3)
        if ( ! fast ) {
            plog("\nPOPULATION MODULE - INFO : Selected model 'BLL'");
            char buffer[300];
            sprintf(buffer, "\n.................. alpha: %g, beta: %g, max_life: %g, exp_life: %g", alpha, beta, max_life, exp_life);
            plog(buffer);
        }
    }

    double uncond_sr(double age); //unconditional survival rate at age / ccdf
    double const_pop_fert(double n); //equilibrium fertility rate for pop of size n.
    double age_of_death(); //produce a random death age for a new person.

};

struct pop_model {
    int model_type; //currently only BLL aka 1
    pop_model_BLL* BLL;  //Boucekkine, Raouf; La Croix, David de; Licandro, Omar (2002): Vintage Human Capital, Demographic Trends, and Endogenous Growth. In Journal of Economic Theory 104 (2), pp. 340–375. DOI: 10.1006/jeth.2001.2854.

    pop_model(const char model[], double t_unit, double par1, double par2)
    {
        if ( 0 == strcmp(model, "BLL") )
            model_type = 1;
        else if ( 0 == strcmp(model, "NONE") )
            model_type = 0;
        else
            model_type = -1;

        switch (model_type) {
            case -1:
                error_hard( "Error in 'pop_model::pop_model()'", "Invalid choice for the population model",
                            "fix your code to prevent this situation." );
                break; //default

            case 0:
                if ( ! fast )
                    plog("\nPOPULATION MODULE - INFO : Selected model 'NONE'");
                break; //none

            case 1:
                BLL = new pop_model_BLL(t_unit, par1, par2);
                break;
        }
    }
    ~pop_model()
    {
        switch (model_type) {
            case 1:
                delete BLL;
                break;
        }
    }


    int init_generations()
    {
        switch (model_type) {
            case 0:
                return 0;
            case 1:
                return int( ceil( BLL->max_life ) );
        }
    }

    double const_pop_fert(double n) //equilibrium fertility rate for pop of size n.
    {
        switch (model_type) {
            case 0:
                return 0.0;
            case 1:
                return BLL->const_pop_fert(n);
        }
    }
    double age_of_death() //produce a random death age for a new person.
    {
        switch (model_type) {
            case 0:
                return -1.0; //no automatic death
            case 1:
                return BLL->age_of_death();
        }
    }


};

class pop_map {
    private:  //internal

        pop_model* model; //hold info on survival function / statistical or data driven population model
        double t_now; //starting time
        double t_unit; //interval size for time
        double femaleShare; //chance to be female

        enum kinship_system {Language, Civil, Canon, Collateral}; //more can be added.
        kinship_system ks = Language; //default

        std::map< int, pop_person > persons;  //all agent information, alive or dead, sorted by their unique ID
        //to do: this is rather slow as it seems. The reason is

        //some functionality to subset alive agents
        std::multimap< double, int > males_by_birth; //helper to efficiently subset persons that are male by age
        std::multimap< double, int > females_by_birth; //same for females
        int n_alive;


        // using agents.emplace_hint(agents.end(),uID,pop_agent() )
        
        const int check_if_person(const int uID); //checks if there is a person for the uID, returning the uID if yes and -1 else.

        void add_person(int uID, int f_uID, int m_uID);
        void person_dies(int uID); //delete from subsets of alive agents
        void person_set_d_age(int uID, double d_age);

        void add_child(int uID, int c_uID);

        object* mother_of(int uID);
        object* father_of(int uID);
        object* first_child_of(int uID);
        object* next_child_of(int uID);
        object* alive_last_mgenitor(int uID); //get alive father of last child. If the father of the last child is dead, return NULL.
        double time_of_last_delivery(int uID);


        double age_of(int uID);
        double d_age_of(int uID);
        bool is_female(int uID);
        bool is_alive(int uID);
        int nchildren(int uID);
        char advance_age_or_die(int uID); //advance age ('a') or die ('d').
        std::string person_info(int uID);

        int family_degree(int id_mother, int id_father, int max_tested_degree = -1); //test how close two persons are in degree.
        bool check_if_incest(int m_uID, int f_uID, int prohibited_degree);

    public:   //external (access via macros)

        pop_map(const char model_type[], double t_start, double t_unit, double maleRatio, double par1 = 0.0, double par2 = 0.0)
            : t_now(t_start), t_unit(t_unit)
        {
            femaleShare = 1.0 - ( maleRatio / ( 1.0 + maleRatio ) );
            model = new pop_model(model_type, t_unit, par1, par2);
            n_alive = 0;
        }
        ~pop_map()
        {
            delete model;
        }

        void set_kinship_system( const char _ks[] );  //Civil, Canon, Collateral

        double const_pop_fert(double n) //equilibrium fertility rate for pop of size n.
        {
            return model->const_pop_fert(n);
        }

        double age_of_death() //produce a random death age for a new person.
        {
            return model->age_of_death();
        }

        int init_generations() //number of generations needed to initialise the model
        {
            return model->init_generations();
        }

        int n_persons()
        {
            return int(persons.size());
        }
        int n_persons_alive()
        {
            return n_alive;
        }

        void add_person(object* uID, object* f_uID, object* m_uID);
        void person_dies(object* uID); //delete from subsets of alive agents
        void person_set_d_age(object* uID, double d_age);
        void advance_time(); //age agents by one unit t_unit, those that get older than age_of_death die.

        object* mother_of(object* uID);
        object* father_of(object* uID);
        object* first_child_of(object* uID);
        object* next_child_of(object* uID);
        object* alive_last_mgenitor(object* uID); //get alive father of last child. If the father of the last child is dead, return NULL.
        double time_of_last_delivery(object* uID);

        object* random_person();

        double age_of(object* uID);
        double d_age_of(object* uID);
        bool is_female(object* uID);
        bool is_alive(object* uID);
        int nchildren(object* uID);

        object* random_person(char gender, double age_low, double age_high, object* fake_caller = NULL, int lag = 0,  const char varLab[] = "", const char condition[] = "", double condVal = 0.0, bool random = true);

        friend struct pop_selection; //functor to select some elements.

        std::string person_info(object* uID);

        int family_degree(object* m_uID, object* f_uID, int max_tested_degree = -1); //test how close two persons are in degree.
        bool check_if_incest(object* m_uID, object* f_uID, int prohibited_degree);

        std::map< int, pop_person >::const_iterator it_persons_begin() //produce an iterator through the map.
        {
            return persons.begin();
        };
        std::map< int, pop_person >::const_iterator it_persons_end() //produce an iterator through the map.
        {
            return persons.end();
        };
};





struct pop_children {
    std::deque<int> c_uIDs; //children, sorted descending by age.
    int child;
};

struct pop_selection {
    pop_map* map;
    char gender; //x : both, f: female, m: male
    double age_low;
    double age_high;
    std::vector< std::pair<double, int> > selection; //rnd for sort, uID
    std::vector< std::pair<double, int> >::iterator it_selection;
    object* first();
    object* next();


    pop_selection( pop_map* map, char gender, double age_low, double age_high, object* fake_caller = NULL, int lag = 0,  const char varLab[] = "", const char condition[] = "", double condVal = 0.0, bool random = true)
        : map(map), gender(gender), age_low(age_low), age_high(age_high)
    {
        const bool has_condition = ! (strlen(condition) == 0);
        if (age_low != -1 && age_high != -1 && age_low > age_high)
            error_hard( "Error in 'pop_selection()'", "age_low > age_high",
                        "Check your code to prevent this situation." );
        double birth_low = age_high > 0 ? map->t_now - age_high : 0;
        double birth_high = age_low > 0 ? map->t_now - age_low : map->t_now;
        // int track = 0;
        if (gender != 'f') { //add males
            auto const& it_start = (birth_low < 0) ? map->males_by_birth.begin() : map->males_by_birth.lower_bound(birth_low);
            auto const& it_stop = (birth_high < 0) ? map->males_by_birth.end() : map->males_by_birth.upper_bound(birth_high);
            for (auto it_temp = it_start; it_temp != it_stop; it_temp++) {
                //check condition, if necessary
                if (has_condition) {
                    object* this_obj = root->obj_by_unique_id(it_temp->second);
                    if (! (this_obj->check_condition( varLab, condition, condVal, fake_caller, lag) ) )
                        continue;
                }//end conditional

                selection.emplace_back(RND, it_temp->second);
            }
        }
        // track = 0;
        if (gender != 'm') { //add females
            auto const& it_start = (birth_low < 0) ? map->females_by_birth.begin() : map->females_by_birth.lower_bound(birth_low);
            auto const& it_stop = (birth_high < 0) ? map->females_by_birth.end() : map->females_by_birth.upper_bound(birth_high);
            for (auto it_temp = it_start; it_temp != it_stop; it_temp++) {
                //check condition, if necessary
                if (has_condition) {
                    object* this_obj = root->obj_by_unique_id(it_temp->second);
                    if (! (this_obj->check_condition( varLab, condition, condVal, fake_caller, lag) ) )
                        continue;
                }//end conditional

                selection.emplace_back(RND, it_temp->second);
            }
        }
        //randomise
        if (true == random) {
            std::sort( selection.begin(),  selection.end(), [](auto const & A, auto const & B ) {
                return A.first < B.first;
            } ); //sort only by rnd
        }
    }
};

struct pop_person {
    bool alive; //true as long as alive
    int uID; //to link to unique id.
    int f_uID; //father
    int m_uID; //mother
    char gender; //0
    double t_birth; //time of birth
    double t_delivery; //time of last delivery of a child
    double age; //current age
    double d_age; //death age
    pop_children children; //functor to cycle through children

    //constructor
    pop_person(int uID, int f_uID, int m_uID, char gender, double t_birth, double age, double d_age) :
        alive(true), uID(uID), f_uID(f_uID), m_uID(m_uID), gender(gender), t_birth(t_birth), age(age), d_age(d_age)
    {
        t_delivery = 0.0;
    }
};
