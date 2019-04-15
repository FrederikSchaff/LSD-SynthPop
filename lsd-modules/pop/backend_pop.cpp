/*************************************************************
    backend_pop.cpp
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

void pop_map::set_kinship_system( const char _ks[] )  //Civil, Canon, Collateral
{
    if ( 0 == strcmp( _ks, "Language") ) {
    ks = Language;
}
else if ( 0 == strcmp( _ks, "Civil") ) {
    ks = Civil;
}
else if ( 0 == strcmp( _ks, "Canon") ) {
    ks = Canon;
}
else if ( 0 == strcmp( _ks, "Collateral") ) {
    ks = Collateral;
}
else {
    error_hard( "failure in set_kinship_system()", "the selected system is not an option",
                "check your code to prevent this situation, options are: Language, Civil, Canon or Collateral" );
    }
}

int pop_map::nchildren(object* uID)
{
    return nchildren( uID->unique_id() );
}

int pop_map::nchildren(int uID)
{
    return int(persons.at(uID).children.c_uIDs.size() );
}

const int pop_map::check_if_person(const int uID){
    if ( persons.end() == persons.find(uID) ){
        return -1; //does not exist
    } else {
        return uID;
    }
}

void pop_map::add_person(object* uID, object* f_uID, object* m_uID)
{
    add_person(uID->unique_id(), f_uID != NULL ? f_uID->unique_id() : -1, m_uID != NULL ? m_uID->unique_id() : -1);
}

void pop_map::add_person(int uID, int f_uID, int m_uID)
{
    //Make sure that female is mother and vv    
    
    if (f_uID > -1) {
        if (check_if_person(f_uID) != f_uID){
            char buffer[300];
            sprintf( buffer, "failure in %s ", __func__ );
            error_hard( buffer, "the first passed uID is not a person",
                    "check your code to prevent this situation" );
        }
        if (persons.at(f_uID).gender != 'm') {
            char buffer[300];
            sprintf( buffer, "failure in %s", __func__ );
            error_hard( buffer, "the father should be male but is not",
                        "check your code to prevent this situation" );
            return;
        }
    }
    if (m_uID > -1) {
        if (check_if_person(m_uID) != m_uID){
            char buffer[300];
            sprintf( buffer, "failure in %s ", __func__ );
            error_hard( buffer, "the second passed uID is not a person",
                    "check your code to prevent this situation" );
        }
        if (persons.at(m_uID).gender != 'f') {
            char buffer[300];
            sprintf( buffer, "failure in %s", __func__ );
            error_hard( buffer, "the mother should be male but is not",
                        "check your code to prevent this situation" );
            return;
        }
    }


    bool female = RND < femaleShare ? true : false;
    const double age = 0.0;
    const double& t_birth = t_now;
    if (f_uID > -1)
        add_child(f_uID, uID);
    if (m_uID > -1)
        add_child(m_uID, uID);
    persons.emplace_hint(persons.end(), uID, pop_person(uID, f_uID, m_uID, (true == female ? 'f' : 'm'), t_birth, age, model->age_of_death() ) );
    ++n_alive;
    if (true == female)
        females_by_birth.emplace_hint(females_by_birth.end(), t_birth, uID);
    else
        males_by_birth.emplace_hint(males_by_birth.end(), t_birth, uID);

    VERBOSE_MODE(TEST_POP_MODULE && uID < 50) {
        char test_msg[300];
        sprintf(test_msg, "\ncalled add_person(). Added person with ID %i, father %i, mother %i, age %g, death age %c, gender %c", persons.at(uID).uID, persons.at(uID).f_uID, persons.at(uID).m_uID, persons.at(uID).age, persons.at(uID).d_age, persons.at(uID).gender);
        plog(test_msg);
    }
}

char pop_map::advance_age_or_die(int uID)
{
    if (persons.at(uID).age == persons.at(uID).d_age) {
        return 'd';
    }
    else {
        persons.at(uID).age += t_unit;
        return 'a';
    }
}

void pop_map::advance_time()
{
    t_now += t_unit;
    std::vector<int> will_die;
    //Cycle through living persons (fe)male, age and delete as necessary.
    //   auto const &next_p = males_by_birth.begin(); //deleting invalidates the current iterator
    std::multimap< double, int >::iterator current_m = males_by_birth.begin(); //deleting invalidates the current iterator
    while (current_m != males_by_birth.end()) {
        int c_uID = current_m->second;
        std::advance(current_m, 1);
        if (advance_age_or_die(c_uID) == 'd')
            will_die.push_back(c_uID);
    }
    std::multimap< double, int >::iterator current_f = females_by_birth.begin();
    while (current_f != females_by_birth.end()) {
        int c_uID = current_f->second;
        std::advance(current_f, 1);
        if (advance_age_or_die(c_uID) == 'd')
            will_die.push_back(c_uID);
    }
    for (auto const& item : will_die) {
        person_dies(item);
    }
}

void pop_map::person_dies(object* uID)
{
    person_dies(uID->unique_id());
}

void pop_map::person_dies(int uID)
{
    if (false == persons.at(uID).alive) {
        error_hard( "Error in 'pop_map::person_dies(int uID)'", "The person that should die is already declared dead",
                    "check your code to prevent this situation." );
        return;
    }

    std::multimap<double, int>::iterator to_del;
    //find first position in (fe)male subset with correct age
    persons.at(uID).alive = false; //set to dead
    if ('f' == persons.at(uID).gender)
        to_del = females_by_birth.find(persons.at(uID).t_birth);
    else
        to_del = males_by_birth.find(persons.at(uID).t_birth);

    //commence to correct person (multiple persons with same age may exist)
    while (to_del->second != uID)
        to_del++;

    //check, can be deactivated if tested sufficiently
    if (to_del->first != persons.at(uID).t_birth) {
        error_hard( "Error in 'pop_map::person_dies(int uID)'", "The person that should die could not be found",
                    "contact the developer." );
        return;
    }

    //erase person from the subset
    if ('f' == persons.at(uID).gender)
        females_by_birth.erase(to_del);
    else
        males_by_birth.erase(to_del);

    --n_alive;

    VERBOSE_MODE(TEST_POP_MODULE && uID < 50) {
        char test_msg[300];
        sprintf(test_msg, "\ncalled person_dies(). Delete person with ID %i, father %i, mother %i, age %g, death age %g, gender %c", uID, persons.at(uID).f_uID, persons.at(uID).m_uID, persons.at(uID).age, persons.at(uID).d_age, persons.at(uID).gender);
        plog(test_msg);
    }

}

void pop_map::person_set_d_age(object* uID, double d_age)
{
    person_set_d_age(uID->unique_id(), d_age);
}

void pop_map::person_set_d_age(int uID, double d_age)
{
    double adjusted_d_age = floor(d_age * t_unit)
                            + ( ( RND < floor(d_age * t_unit) - d_age * t_unit ) ? t_unit : 0.0  );

    VERBOSE_MODE(TEST_POP_MODULE && uID < 50) {
        char test_msg[300];
        sprintf(test_msg, "\ncalled person_set_d_age(). Adjust age of person ID %i with age %g and former death age %g to new death age %g (t_unit %g).",
                uID, persons.at(uID).age, persons.at(uID).d_age, adjusted_d_age, t_unit);
        plog(test_msg);
    }

    persons.at(uID).d_age = adjusted_d_age;
}

void pop_map::add_child(int uID, int c_uID)
{
    persons.at(uID).children.c_uIDs.push_back(c_uID);
    persons.at(uID).t_delivery = persons.at(uID).t_birth + persons.at(uID).age;
}

object* pop_map::mother_of(object* uID)
{
    return mother_of( uID->unique_id() );
}

object* pop_map::mother_of(int uID)
{
    int m_uID = persons.at(uID).m_uID;
    if (m_uID > -1)
        return root->obj_by_unique_id( m_uID );
    return NULL; //no father case
}

object* pop_map::father_of(object* uID)
{
    return father_of( uID->unique_id() );
}

object* pop_map::father_of(int uID)
{
    int f_uID = persons.at(uID).f_uID;
    if (f_uID > -1)
        return root->obj_by_unique_id( f_uID );
    return NULL; //no father case
}

object* pop_map::first_child_of(object* uID)
{
    return first_child_of( uID->unique_id() );
}

object* pop_map::first_child_of(int uID)
{
    //initialisation
    persons.at(uID).children.child = 0;
    //get child
    return next_child_of(uID);
}

object* pop_map::next_child_of(object* uID)
{
    return next_child_of( uID->unique_id() );
}

object* pop_map::next_child_of(int uID)
{
    if (persons.at(uID).children.child < persons.at(uID).children.c_uIDs.size())
        return root->obj_by_unique_id( persons.at(uID).children.c_uIDs.at( persons.at(uID).children.child++ ) );
    return NULL;
}

//get alive father of last child. If the father of the last child is dead, return NULL.
object* pop_map::alive_last_mgenitor(object* uID)
{
    return alive_last_mgenitor(uID->unique_id());
}

object* pop_map::alive_last_mgenitor(int uID)
{
    if (persons.at(uID).children.c_uIDs.size() == 0) {
        return NULL; //no children yet, hence no father
    }
    else {
        int lastchild = persons.at(uID).children.c_uIDs.back();
        int lastmgenitor = persons.at(lastchild).f_uID;
        if ( lastmgenitor == -1 ) {
            return NULL; //no father
        }
        else {
            if (true == persons.at(lastmgenitor).alive) {
                return root->obj_by_unique_id(lastmgenitor); //found the father
            }
            else {
                return NULL; //former male genitor is dead
            }
        }
    }
}

//get alive father of last child. If the father of the last child is dead, return NULL.
double pop_map::time_of_last_delivery(object* uID)
{
    return time_of_last_delivery(uID->unique_id());
}

double pop_map::time_of_last_delivery(int uID)
{
    return persons.at(uID).t_delivery;
}

double pop_map::age_of(object* uID)
{
    return  age_of( uID->unique_id() );
}

double pop_map::age_of(int uID)
{
    return persons.at(uID).age;
}

double pop_map::d_age_of(object* uID)
{
    return  d_age_of( uID->unique_id() );
}

double pop_map::d_age_of(int uID)
{
    return persons.at(uID).d_age;
}

bool pop_map::is_female(object* uID)
{
    return  is_female( uID->unique_id() );
}

bool pop_map::is_female(int uID)
{
    return ('f' == persons.at(uID).gender);
}

bool pop_map::is_alive(object* uID)
{
    return  is_alive( uID->unique_id() );
}

bool pop_map::is_alive(int uID)
{
    return persons.at(uID).alive;
}


object* pop_selection::first()
{
    it_selection = selection.begin();
    if (it_selection == selection.end())
        return NULL;
    else
        return root->obj_by_unique_id(it_selection->second);
}

object* pop_selection::next()
{
    if (++it_selection == selection.end() )
        return NULL;
    else
        return root->obj_by_unique_id(it_selection->second);
}

object* pop_map::random_person(char gender, double age_low, double age_high, object* fake_caller, int lag, const char varLab[], const char condition[], double condVal, bool random)
{
    return  pop_selection(this, gender, age_low, age_high, fake_caller, lag, varLab, condition, condVal, random).first();
}


std::string pop_map::person_info(object* uID)
{
    return person_info(uID->unique_id());
}

std::string pop_map::person_info(int uID)
{
    std::string buffer =  "\n(POP PERSON INFO) t="+std::to_string(t)+": ";
    pop_person const& POI = persons.at(uID);
    object* oPOI = root->obj_by_unique_id(uID);
    buffer += " uID: "      + std::to_string(POI.uID);
    buffer += ", Status: "   + std::string(POI.alive ? "alive" : "dead");
    buffer += ", gender: ";
    buffer += ( (POI.gender == 'f') ? "female" : "male");
    buffer += ", age: "      + std::to_string(POI.age);
    buffer += ", age of death: "  + std::to_string(POI.d_age);
    buffer += ", mother: "   + std::to_string(POI.m_uID);
    buffer += ", father: "   + std::to_string(POI.f_uID);

    if (ANY_GISS(oPOI))
        buffer += oPOI->gis_info(true); //append mode
    return buffer;
}


/*  a function that returns the family degree, defined by the kinship_system used.
    see https://umanitoba.ca/faculties/arts/anthropology/tutor/descent/cognatic/degree.html for infos.

    retunrs the degree of kinship according to the given system.
    a return value of -1 - flags no testing necessary. -2 is an error.

    In addition to Civil, Canon and Collateral, "Language" refers to the following degrees:

    0 - none of the below/tested ones
    1 - parent-child
    2 - siblings
    3 - grandchild-grandparent
    4 - nephew - uncle/aunt
    5 - cousin - cousin

    Implementation: Problem specific
*/
int pop_map::family_degree(object* m_uID, object* f_uID, int max_tested_degree)
{
    return family_degree(m_uID->unique_id(), f_uID->unique_id(), max_tested_degree);
}

//Test degree between to persons, potential "mother" and potential "father" (or otherwise)
int pop_map::family_degree(int m_uID, int f_uID, int max_tested_degree)
{

    if (max_tested_degree == 0) {
        return -1;     //no testing
    }

    const bool verbose_logging = false;

    VERBOSE_MODE(verbose_logging) {
        LOG("\nPopulation Model :   : ext_pop::check_if_incest : called with mother %i, father %i and max degree %i", m_uID, f_uID, max_tested_degree);
    }

    const int max_testable_degree = 5; //in Language System. Currently cousin relations are the most distant relation that is checked.

    if( max_tested_degree > max_testable_degree ) {
        char buffer[300];
        sprintf( buffer, "failure in %s ", __func__ );
        error_hard( buffer, "the maximum tested degree is higher than allowed (5 - cousins )",
                    "check your code to prevent this situation or contact the developer" );
        return -2;
    }

    //Steps to lowest common ancestor (LCA)
    int steps_m = -1; //flag that outside of checked distance.
    int steps_f = -1;
    int steps_Language = -1;
    int tested_degree = 0;
    pop_person* c_mother = NULL; //default: no mother
    pop_person* c_father = NULL;

    if ( -1 == max_tested_degree) {
        max_tested_degree = max_testable_degree;   //default max - we would need to expand this heuristic approach otherwise. This is based on the language definition.
    }


    //test if orphan
    if (m_uID < 0 && f_uID < 0) { //no parents

        VERBOSE_MODE(verbose_logging) {
            LOG("\nt .. full orphan");
        }
        goto at_end; //full orphan
    }

    //set-up
    if (m_uID > -1) {
        c_mother = &persons.at(m_uID);
    }
    if (f_uID > -1) {
        c_father = &persons.at(f_uID);
    }


    //Start serious testing

    //mother-son?
    if (c_mother->f_uID != -1 && c_mother->f_uID == f_uID) {
        steps_Language = 1;
        steps_m = 1;
        steps_f = 0;
        goto at_end;
    }

    //father-daughter?
    if ( c_father->m_uID != -1 && c_father->m_uID == m_uID) {
        steps_Language = 1;
        steps_m = 0;
        steps_f = 1;
        goto at_end;
    }
    
    VERBOSE_MODE(verbose_logging) {
        LOG("\n\t ... not parent-child");
    }

    tested_degree++;
    if (max_tested_degree == tested_degree) {
        goto at_end;
    }

    //mother siblings
    if (c_mother->m_uID != -1 && c_mother->m_uID == c_father->m_uID) {
        steps_Language = 2;
        steps_m = steps_f = 1;
        goto at_end;
    }
    //father siblings
    if (c_mother->f_uID != -1 && c_mother->f_uID == c_father->f_uID) {
        steps_Language = 2;
        steps_m = steps_f = 1;
        goto at_end;
    }
    
    VERBOSE_MODE(verbose_logging) {
        LOG(", nor siblings");
    }

    tested_degree++;
    if (max_tested_degree == tested_degree) {
        goto at_end;
    }

    //grandchild-grandparent?
    if (/* grandchild-grandpas */
        (c_mother->f_uID != -1 && persons.at(c_mother->f_uID).f_uID != -1 && persons.at(c_mother->f_uID).f_uID == f_uID)
        || (c_mother->m_uID != -1 && persons.at(c_mother->m_uID).f_uID != -1 && persons.at(c_mother->m_uID).f_uID == f_uID)
    ) {
        steps_Language = 3;
        steps_m = 2;
        steps_f = 0;
        goto at_end;
    }

    if (/*grandson-grandmas */
        (c_father->f_uID != -1 && persons.at(c_father->f_uID).m_uID != -1 && persons.at(c_father->f_uID).m_uID == m_uID)
        || (c_father->m_uID != -1 && persons.at(c_father->m_uID).m_uID != -1 && persons.at(c_father->m_uID).m_uID == m_uID)
    ) {
        steps_Language = 3;
        steps_m = 0;
        steps_f = 2;
        goto at_end;
    }

    VERBOSE_MODE(verbose_logging) {
        LOG(", nor grandchild-grandparent");
    }
    tested_degree++;
    if (max_tested_degree == tested_degree) {
        goto at_end;
    }

    //Aunt/Uncle - Nephew
    if (/*niece-uncles*/
        (c_mother->f_uID != -1 && persons.at(c_mother->f_uID).f_uID != -1 && c_father->f_uID != -1 && c_father->f_uID == persons.at(c_mother->f_uID).f_uID)
        ||  (c_mother->f_uID != -1 && persons.at(c_mother->f_uID).m_uID != -1 && c_father->m_uID != -1 && c_father->m_uID == persons.at(c_mother->f_uID).m_uID)
        ||  (c_mother->m_uID != -1 && persons.at(c_mother->m_uID).f_uID != -1 && c_father->f_uID != -1 && c_father->f_uID == persons.at(c_mother->m_uID).f_uID)
        ||  (c_mother->m_uID != -1 && persons.at(c_mother->m_uID).m_uID != -1 && c_father->m_uID != -1 && c_father->m_uID == persons.at(c_mother->m_uID).m_uID)
    ) {
        steps_Language = 4;
        steps_m = 1;
        steps_f = 2;
        goto at_end;
    }
    if (/*aunt-nephews*/
        (c_mother->f_uID != -1 && c_father->f_uID != -1 && persons.at(c_father->f_uID).f_uID != -1 && c_mother->f_uID == persons.at(c_father->f_uID).f_uID)
        ||  (c_mother->f_uID != -1 && c_father->m_uID != -1 && persons.at(c_father->m_uID).f_uID != -1 && c_mother->f_uID == persons.at(c_father->m_uID).f_uID)
        ||  (c_mother->m_uID != -1 && c_father->f_uID != -1 && persons.at(c_father->f_uID).m_uID != -1 && c_mother->m_uID == persons.at(c_father->f_uID).m_uID)
        ||  (c_mother->m_uID != -1 && c_father->m_uID != -1 && persons.at(c_father->m_uID).m_uID != -1 && c_mother->m_uID == persons.at(c_father->m_uID).m_uID)
    ) {
        steps_Language = 4;
        steps_m = 2;
        steps_f = 1;
        goto at_end;
    }
    VERBOSE_MODE(verbose_logging) {
        LOG(", nor aunt/uncle-niece/nephew");
    }
    tested_degree++;
    if (max_tested_degree == tested_degree) {
        goto at_end;
    }

    //Cousins
    if (
        (/*cousins*/
            (c_mother->f_uID != -1 && persons.at(c_mother->f_uID).f_uID != -1 && c_father->f_uID != -1 && persons.at(c_father->f_uID).f_uID != -1 && persons.at(c_father->f_uID).f_uID == persons.at(c_mother->f_uID).f_uID)
            ||  (c_mother->f_uID != -1 && persons.at(c_mother->f_uID).m_uID != -1 && c_father->f_uID != -1 && persons.at(c_father->f_uID).m_uID != -1 && persons.at(c_father->f_uID).m_uID == persons.at(c_mother->f_uID).m_uID)
            ||  (c_mother->m_uID != -1 && persons.at(c_mother->m_uID).f_uID != -1 && c_father->f_uID != -1 && persons.at(c_father->f_uID).f_uID != -1 && persons.at(c_father->f_uID).f_uID == persons.at(c_mother->m_uID).f_uID)
            ||  (c_mother->m_uID != -1 && persons.at(c_mother->m_uID).m_uID != -1 && c_father->f_uID != -1 && persons.at(c_father->f_uID).m_uID != -1 && persons.at(c_father->f_uID).m_uID == persons.at(c_mother->m_uID).m_uID)
        ) ||
        (/*         */
            (c_mother->f_uID != -1 && persons.at(c_mother->f_uID).f_uID != -1 && c_father->m_uID != -1 && persons.at(c_father->m_uID).f_uID != -1 && persons.at(c_father->m_uID).f_uID == persons.at(c_mother->f_uID).f_uID)
            ||  (c_mother->f_uID != -1 && persons.at(c_mother->f_uID).m_uID != -1 && c_father->m_uID != -1 && persons.at(c_father->m_uID).m_uID != -1 && persons.at(c_father->m_uID).m_uID == persons.at(c_mother->f_uID).m_uID)
            ||  (c_mother->m_uID != -1 && persons.at(c_mother->m_uID).f_uID != -1 && c_father->m_uID != -1 && persons.at(c_father->m_uID).f_uID != -1 && persons.at(c_father->m_uID).f_uID == persons.at(c_mother->m_uID).f_uID)
            ||  (c_mother->m_uID != -1 && persons.at(c_mother->m_uID).m_uID != -1 && c_father->m_uID != -1 && persons.at(c_father->m_uID).m_uID != -1 && persons.at(c_father->m_uID).m_uID == persons.at(c_mother->m_uID).m_uID)
        )
    ) {
        steps_Language = 5;
        steps_m = steps_f = 2;
        goto at_end;
    }
    VERBOSE_MODE(verbose_logging) {
        LOG(", nor cousins");
    }
    tested_degree++;
    if (max_tested_degree == tested_degree) {
        goto at_end;
    }

at_end: //label that at end

    if (steps_m == -1) {
        return 0; //not related within given distance
    }
    else {
        switch (ks) {
            case Language:
                return steps_Language;
            case Civil:
                return steps_f + steps_m;

            case Canon:
                return max(steps_f, steps_m);

            case Collateral:
                return min(steps_f, steps_m);
        }
    }
}

// Check the family degree of the relationship
//Check if there is potential of incest with given "degree" - only direct biology.
// allowed degree: 0 - not checked,
// in "Language" system: 1 - parent-child, 2 - also siblings,   3 - also grandchild-grandparent, 4 - also nephew - uncle/aunt, 5 - also cousin - cousin.
// we compare be-directional, to also catch if a mother would have a child with a (grand)child.
// returns true if there is incest
bool pop_map::check_if_incest(object* m_uID, object* f_uID, int prohibited_degree)
{
    return check_if_incest(m_uID->unique_id(), f_uID->unique_id(), prohibited_degree);
}

bool pop_map::check_if_incest(int m_uID, int f_uID, int prohibited_degree)
{
    const bool verbose_logging = false;
    if (prohibited_degree == 0) {
        return false; //no prohibited incest.
    }
    int kinship_degree = family_degree(m_uID, f_uID, prohibited_degree);

    if (kinship_degree >= prohibited_degree) {
        VERBOSE_MODE(verbose_logging) {
            LOG("\nPopulation Model :   : ext_pop::check_if_incest : Maximum forbidden degree is %i", prohibited_degree);
            switch (kinship_degree) {

                case 0:
                    LOG("\n\t... No relevant family relation. ERROR this should not happen.");
                    break;
                case 1:
                    LOG("\n\t... parent-child.");
                    break;
                case 2:
                    LOG("\n\t... siblings.");
                    break;
                case 3:
                    LOG("\n\t... grandchild-grandparent.");
                    break;
                case 4:
                    LOG("\n\t... nephew - uncle/aunt.");
                    break;
                case 5:
                    LOG("\n\t... cousin - cousin.");
                    break;
            }
        }
        return true; //incest
    }
    else {
        return false;
    }
}

double pop_model_BLL::uncond_sr(double age) //chance to live up to year.
{
    //bll (1), survival function
    return (exp(-beta * age) - alpha) / (1.0 - alpha);
}

double pop_model_BLL::const_pop_fert(double n) //fertility rate to keep population constant if in equilibrium.
{
    //inverse of life expectancy times n
    double raw = (n / exp_life) * t_unit;
    double adjusted = floor(raw) + ( RND < (raw - floor(raw) ) ? 1.0 : 0.0);
    return adjusted;
}

double pop_model_BLL::age_of_death()
{
    double raw =  - ( log( RND * (1 - alpha) + alpha ) / beta ); //from solving m(a) for a.
    double prob = raw * t_unit - floor(raw * t_unit);
    double adjusted = floor(raw * t_unit) / t_unit + ( RND < prob ? 1.0 : 0.0 );
    //LOG("\nRaw %g, prob %g, adjusted %g",raw,prob,adjusted);
    return adjusted;
}
