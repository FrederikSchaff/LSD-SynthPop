//#define EIGENLIB			// uncomment to use Eigen linear algebra library
//#define NO_POINTER_INIT	// uncomment to disable pointer checking

#include "fun_head_fast.h"


/******************************************************************************/
/* Some debugging tools.                                                      */
/* (un)comment to switches to turn capabilities on(off)                       */
/* if commented, no loss in performance                                       */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
// #define SWITCH_TEST_OFF   //(un)comment to switch on(off)
// #define SWITCH_VERBOSE_OFF  //(un)comment to switch on(off)
#define TRACK_SEQUENCE_MAX_T 1000 //number of steps for tracking
#define SWITCH_PAJEK_OFF //(un)comment to switch on(off)
//#define SWITCH_TRACK_SEQUENCE_ALL //(un)comment to switch off(on) tracking all variable calls
#define DISABLE_LOCAL_CLOCKS
//#define USE_ASSERTS


/******************************************************************************/
/*         Population model backend                                           */
/*  ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "lsd-modules/debug-tools/validate.h"
#ifndef SWITCH_PAJEK_OFF
#include "lsd-modules/PajekFromCpp/PajekFromCpp_head.h" //before backend!
#endif
#include "lsd-modules/pop/backend_pop_LSD.h"

//#define USE_LAT

/* -------------------------------------------------------------------------- */

object* sense = NULL; //We may follow a single user.

MODELBEGIN


/*----------------------------------------------------------------------------*/
/*           General Helpers                                                  */
/*----------------------------------------------------------------------------*/

////////////////////////
EQUATION("Updating_Scheme")
/*  Controls the flow of events at any single simulation tick. This should always
    be the first equation called in any model! */
//TRACK_SEQUENCE
double updating_scheme_ret = 0.0;
{
    V("Init_Global"); //once then parameter

    /* Before all else, the population is updated */
    V("Pop_age");     //existing generation ages. Some persons may die.
    V("Pop_birth");   //If the population model generates new agents, that happens here.

}
RESULT(updating_scheme_ret)

////////////////////////
EQUATION("Init_Global")
/*  This it the main initialisation function, calling all initialisation action
    necessary. */
double init_global_ret = 0.0;
{
    FAST;
    int model_type = V("Model_Type"); //allow testing different models.
    const double maleRatio = 1.048; //males per female
    int n_generation = 0;    


    if (model_type == 1) {
        INIT_POPULATION_MODULE("BLL", 0.0, 1.0, maleRatio, V("m1_alpha"), V("m1_beta")); //Model, t_start, t_unit, maleRatio, par1, par2
        POP_SET_FAMILY_SYSTEM("Language");
        n_generation = POP_CONSTN_BIRTH(V("Pop_const_n")); //size of the first generation
    }
    else {
        INIT_POPULATION_MODULE("NONE", 0.0, 1.0, 0.5); //Model, t_start, t_unit, maleRatio
        n_generation = V("Pop_const_n") / V("m0_maxLife") * 2;
    }

    int grid_size = max ( 10, ceil( sqrt( V("Pop_const_n") / 10 ) ) ) ;
#ifdef USE_LAT
    grid_size = max ( 10, ceil( sqrt( V("Pop_const_n") * 10 ) ) ) ; //better visualisation
#endif

    PLOG("\nSelected underlying grid size is %d", grid_size);
    INIT_SPACE_ROOT_WRAP(grid_size, grid_size, 15); //initialise grid space as in BHSC The Grid Size needs to be squared, but is otherwise just a performance parameter.
    SET_GIS_DISTANCE_TYPE('c'); //Chebychev, moore neighbourhood
#ifdef USE_LAT
    INIT_LAT_GISS(root, 0);
#endif

    object* to_delete = SEARCH("Agent"); //Template person will die.
    V("Pop_birth"); // a new agent is added NOW.
    DELETE(to_delete);

    PARAMETER
}
RESULT(init_global_ret)

/******************************************************************************/
/*           Template Model Start - see description.txt                   */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

EQUATION("age")
//TRACK_SEQUENCE
double age_ret = 0.0;
{
    age_ret = POP_AGE;
}
RESULT(age_ret)

EQUATION("female")
//TRACK_SEQUENCE
double female_ret = 0.0;
{
    female_ret = (POP_FEMALE == true) ? 1.0 : 0.0;
    PARAMETER
}
RESULT(female_ret)

EQUATION("ID")
double ID_ret = 0.0;
{
    ID_ret = UID;
    PARAMETER
}
RESULT(ID_ret)

EQUATION("mother")
/* Provides the ID of the mother, if exists. -1.0 else. */
double mother_ret = -1.0;
{
    cur = POP_MOTHER;
    if (cur != NULL)
        mother_ret = UIDS(cur);
    PARAMETER
}
RESULT(mother_ret)

EQUATION("father")
/* Provides the ID of the father, if exists. -1.0 else. */
double father_ret = -1.0;
{
    father_ret = -1;
    cur = POP_FATHER;
    if (cur != NULL)
        father_ret = UIDS(cur);
    PARAMETER
}
RESULT(father_ret)

EQUATION("nChildren")
/* Count number of children */
double nChildren_ret = POP_NCHILDREN;
RESULT(nChildren_ret)

EQUATION("Death")
/* Kill the agent */
//TRACK_SEQUENCE
const double death_ret = 0.0;
{
    if (sense == p) {
        sense = NULL;
        LOG("\n%i\tAgent %g dies and is no more followed.\n--------\n**********\n",t, UID);
    }

    DELETE(p);
    //   PLOG(POP_INFO)
}
RESULT(death_ret)

EQUATION("IsFertile")
/*  Determine if the agent can become a parent based on its age
    and if it is female also based on the time of the last delivery.*/

double isfertile_ret = 0.0;
{
    double age = POP_AGE;
    double timeToNewBirth = V("timeToNewBirth");
    if (true == POP_FEMALE) {
        if ( age >= V("fertility_f_low") && age <= V("fertility_f_high")
                && POP_LASTDELIVERY + timeToNewBirth < T ) {
            isfertile_ret = 1.0;
        }
    }
    else { //male
        if ( age >= V("fertility_m_low") && age <= V("fertility_m_high") ) {
            isfertile_ret = 1.0;
        }
    }

#ifdef USE_LAT
    if (isfertile_ret == 1.0) {
        SET_LAT_PRIORITY(2);
        if (true == POP_FEMALE)
            SET_LAT_COLOR(COLOR_LAT("Pink"));
        else
            SET_LAT_COLOR(COLOR_LAT("Blue2"));
    }
    else {
        if (true == POP_FEMALE && age >= V("fertility_f_high") ) {
            SET_LAT_PRIORITY(0);
            SET_LAT_COLOR(COLOR_LAT("Gray"));
        }
        else if (age >= V("fertility_m_high") ) {
            SET_LAT_PRIORITY(0);
            SET_LAT_COLOR(COLOR_LAT("Gray"));
        }
    }
#endif
}
RESULT(isfertile_ret)

EQUATION("PotentialMother")
/*  Function. Provides information on whether the women can become a mother.
    It needs to be within fertility range, have a partner within fertility range
    and the delivery of the last child needs be long enough in the past. */

//Find_Partner ensures that it is first looked for a partner. The status is reported.
double potentialMother_ret = 0.0;
{
    bool isFertile = ( V("IsFertile") == 1.0 );
    bool hasPartner = ( V("Find_Partner") == 1.0 );
    if (isFertile && hasPartner) {
        potentialMother_ret = 1.0;
    }
}
RESULT(potentialMother_ret)


EQUATION("Pop_birth")
/* Add next generation.*/
double pop_birth_ret = 0;
{
    //TRACK_SEQUENCE
    int model_type = V("Model_Type"); //e.g., 1 == BLL
    int n_generation = 0;
    if (model_type == 1) {
        n_generation = POP_CONSTN_BIRTH(V("Pop_const_n")); //size of the first generation
    }
    else {
        n_generation = V("Pop_const_n") / V("m0_maxLife") * 2;
    }

    pop_birth_ret = n_generation; //just to save this info

    //Add new agents
    //first: use set of potential mothers in random order
    POP_RCYCLE_PERSON_CND(cur, 'f', V("fertility_f_low"), V("fertility_f_high"), "PotentialMother", "=", 1.0) {
        if (n_generation == 0) {
            break; //done.
        }
        V_CHEAT("Add_Agent", cur);
        --n_generation;
    }
    //second: left-overs
    for (int i = 0; i < n_generation; i++) {
        V_CHEAT("Add_Agent", root);
    }

    WRITE("StorckChildRate", (double)n_generation / pop_birth_ret);
}
RESULT( pop_birth_ret )

EQUATION("Add_Agent")
/*  An equation that is called via fake-caller (thus passing the mother) and creates a new agent.
    If the fake-caller is root, there are no parents.
*/

double add_agent_ret = CURRENT + 1; //#persons ever added
{
    object* ptrAgent = ADDOBJLS(p->up, "Agent", 0); //create LSD agent
    //Initialise infos
    WRITES(ptrAgent, "PartnerID", -1.0);

    object* mother = c; //caller
    object* father = NULL; //place holder - no father (yet)

    if (mother != root) {
        father = SEARCH_UID(VS(mother, "PartnerID"));
        if (father == NULL)
            PLOG("\nERROR! See Add_Agent");
    }
    else {
        mother = NULL; //set back to NULL
    }

    POP_ADD_PERSON_WPARENTS(ptrAgent, father, mother); //Add to population module

    if (sense == NULL) {
        sense = ptrAgent;
        LOG("\n--------\n**********\nt=%i\tAgent %g is born and followed from now on.",t, UIDS(ptrAgent));
    }

    //Add to space for wedding ring model
    if (NULL != father && NULL != mother) {
        double x_offset = 2 * V("distance_wd");
        if (RND > 0.5)
            x_offset *= -1;
        double y_offset = 2 * V("distance_wd");
        if (RND > 0.5)
            y_offset *= -1;
        x_offset += POSITION_XS(mother);
        y_offset += POSITION_YS(mother);

        ADD_TO_SPACE_XYS(ptrAgent, root, ABSOLUTE_DISTANCES(root, x_offset), ABSOLUTE_DISTANCES(root, y_offset) );
    }
    else {
        ADD_TO_SPACE_RNDS(ptrAgent, root); //Add to space at random position
    }

#ifdef USE_LAT
    SET_LAT_PRIORITYS(ptrAgent, 0);
    SET_LAT_COLORS(ptrAgent, (mother == NULL ? 1000 : COLOR_LAT("Green") ) );
#endif
    if (V("Model_Type") == 0) {
        POP_SET_DAGES(ptrAgent, RND * V("m0_maxLife")); //in the no model case, chose random age between 0 and 100.
    }
    VS(ptrAgent, "Init_Agent");
}
RESULT( add_agent_ret ) //Number of new persons created.

///////////////////////////////
EQUATION("Pop_age")
/* Each agents get older one year. */
//TRACK_SEQUENCE
double pop_age_ret = CURRENT; //number of persons alive
{
    POP_ADVANCE_TIME //let agents age
    //Kill agents that are dead
    int alive = 0;
    CYCLE_SAFES(p->up, cur, "Agent") {
        if (false == POP_ALIVES(cur)) {
            if (0 < alive || NULL != cur->next ) {
                VS(cur, "Death");
            }
            else {
                PLOG("\nAt time %i: Simulation at premature end. Last agent would have died.", t);
                ABORT
            }
        }
        else {
            alive++;
        }
    }
    pop_age_ret = alive;
}
RESULT(pop_age_ret)


/***** Now the wedding ring model ****/

EQUATION("Init_Agent")
/*  Initialise a new agent
    - define agent characteristics
*/
double init_agent_ret = T;
{
    int ro_type = uniform_int(1, 5); //kind of shift of age interval for relevant others.
    double gamma = RND * V("wr_gamma"); //strength of shift
    double hwidth = RND * V("wr_hintvl"); //size of half of interval

    double a_center;
    switch (ro_type) {
        case 1:
            a_center = - gamma;
            break;
        case 2:
            a_center = - gamma / 2.0;
            break;
        case 3:
            a_center = 0.0;
            break;
        case 4:
            a_center = gamma / 2.0;
            break;
        case 5:
            a_center = gamma;
            break;
    }
    WRITE("ro_a_low", a_center - hwidth); //absolute distance to age younger relevant others
    WRITE("ro_a_high", a_center + hwidth);
    PARAMETER
}
RESULT(init_agent_ret)


EQUATION("psearch_radius")
/*  This is the search radius for the partner search in the Wedding Ring model.
    It is set in 0,1 (relative)
*/
//TRACK_SEQUENCE
// END_EQUATION(1.0);
double psearch_radius_ret = 0.0;
{
    double sp = V("Social_Pressure");
    double ai_P = V("age_influence");
    double distance_wd = V("distance_wd");

    psearch_radius_ret = sp * ai_P * distance_wd;
}
RESULT(psearch_radius_ret)

EQUATION("age_accept_low")
/* Calculate the lower bound of acceptable age */
double age_accept_low_ret = 0.0;
{
    bool alt_model = V("WD_alt_model") == 0.0 ? false : true; //define social pressure on share of people married, as in the original, or on share of people with children (true).
    age_accept_low_ret = POP_AGE - V("Social_Pressure") * V("age_influence") * V("c_WR");
    if (alt_model) {
        age_accept_low_ret = max( age_accept_low_ret, ( POP_FEMALE ? V("fertility_m_low") : V("fertility_f_low") ) );
    }
}
RESULT(age_accept_low_ret)

EQUATION("age_accept_high")
/* Calculate the lower bound of acceptable age */
double age_accept_high_ret = 0.0;
{
    bool alt_model = V("WD_alt_model") == 0.0 ? false : true; //define social pressure on share of people married, as in the original, or on share of people with children (true).
    age_accept_high_ret = POP_AGE + V("Social_Pressure") * V("age_influence") * V("c_WR");
    if (alt_model) {
        age_accept_high_ret = min( age_accept_high_ret, ( POP_FEMALE ? V("fertility_m_high") : V("fertility_f_high") ) );
    }
}
RESULT(age_accept_high_ret)

EQUATION("age_influence")
/*  A factor that decides on the size of the socio-spatial network based on the age of the person. */
//TRACK_SEQUENCE
// END_EQUATION(1.0);
double age_influence_ret = 0.0;
{
    double age = POP_AGE;

    if ( age > 64 ) {
        age_influence_ret = 0.1;
    }
    else if ( age > 60 ) {
        age_influence_ret = 6.5 - 0.1 * age;
    }
    else if ( age > 38 ) {
        age_influence_ret = 0.5;
    }
    else if ( age > 33 ) {
        age_influence_ret = 4.3 - 0.1 * age;
    }
    else if ( age > 20 ) {
        age_influence_ret = 0.1;
    }
    else if ( age >= 16 ) {
        age_influence_ret = 0.2 * age - 3.1;
    }
    else {
        age_influence_ret = 0.0;
    }
}
RESULT(age_influence_ret)

EQUATION("Social_Pressure")
/*  This is the social presure that rests upon the individual to get a partner and start getting (more) children
    At this point, the parameters alpha and beta are hard coded as in the papers [2,3], because the parameters it self are not useful to interprete.
    
    1) Define the set of relevant others
    2) Calculate the percantage of married persons / persons with children
    3) Calculate the resulting social pressure    
*/
SET_LOCAL_CLOCK_RF
ADD_LOCAL_CLOCK_TRACKSEQUENCE
// END_EQUATION(1.0);
double social_pressure_ret = 0.0;
{
    bool alt_model = V("WD_alt_model") == 0.0 ? false : true; //define social pressure on share of people married, as in the original, or on share of people with children (true).
    double pocm = 0.0; // (percantage) others with children / married
    double total = 0.0;
    double age_low = POP_AGE - V("ro_a_low"); //relevant others minimum age
    double age_high = POP_AGE + V("ro_a_high");
    double rho = V("rho_WD"); //chance to take
    int ndistance = V("n_social"); //number of people in social network.    
    double d_social = 0.0; //the effective social distance in relative Chebychev distance.
    
    int neighbours = 0;
    //In a first step, calculate size of set of relevant others
    if (p == sense) {
        TRACK_SEQUENCE;
        LOG(POP_INFO);
        LOG("\n Searching among nearest %i neighbours (lower bound)", ndistance);        
        LOG("\n age_low is %g, age_high is %g, rho is %g\n", age_low, age_high, rho);
    }
    cur1 = NULL;
    //FCYCLE_NEIGHBOUR(cur, "Agent", ABSOLUTE_DISTANCE( V("distance_wd") ) ) { //first check distance
    NFCYCLE_NEIGHBOUR(cur, "Agent", ndistance, -1 ) { //first check distance
        ++neighbours;
        //Check if relevant other
        if ( POP_AGES(cur) < age_low   //skip if to young
                || POP_AGES(cur) > age_high //skip if to old
                || rho < RND ) { //skip by chance
            // if (p == sense && POP_AGE > 15 && POP_AGE < 40) {
                // PLOG(POP_INFOS(cur));
                // PLOG(" -- Skipped");
            // }
            continue; //skip
        }

        total++;

        //check if putting pressure
        if (alt_model && POP_NCHILDRENS(cur) > 0 ) {
            ++pocm; //with children
        }
        else if ( VS(cur, "Partner_Status") == 1.0 ) {
            ++pocm; //married
        }
        cur1 = cur; //item furthest away.
    }
    if (cur1 != NULL)
        d_social = RELATIVE_DISTANCE(DISTANCE(cur1));
    WRITE("d_social",d_social);

    const double beta = 7.0; //V("beta_WR");
    const double alpha = 0.5;// V("alpha_WR");
    double temp = exp(beta * (pocm - alpha));
    WRITE("nrelevant_others", pocm); //size of the set of relevant others
    social_pressure_ret = temp / (1 + temp);
    social_pressure_ret = max (social_pressure_ret, V("min_sp_wd") );  //minimum social pressure.
    if (p == sense) {
        LOG("\nTotal is %g from %i (>= ndistance %i, effective distance %g). Social pressure is %g (pocm %g) ", total, neighbours, ndistance, social_pressure_ret,pocm,d_social);
    }
}
REPORT_LOCAL_CLOCK_CND(0.02);
RESULT(social_pressure_ret)

EQUATION_DUMMY( "nrelevant_others", "Social_Pressure" ) //Make sure that Social Pressure is calculated before nrelevant_others is used.

EQUATION("Statistics")
/* Some basic cross section stats */
double statistics_ret = 0.0;
SET_LOCAL_CLOCK_RF
ADD_LOCAL_CLOCK_TRACKSEQUENCE
USE_NAN {
    STAT("Social_Pressure");
    WRITE("SP_avg", v[1]);
    WRITE("SP_sd", v[2] > 0 ? sqrt(v[2]) : NAN );
    WRITE("SP_max", v[3]);
    WRITE("SP_min", v[4]);
    statistics_ret = v[0];

    // STAT("AvgKinshipDegree");
    // WRITE("AvgKinshipDegree_avg", v[1]);
    // WRITE("AvgKinshipDegree_sd", v[2] > 0 ? sqrt(v[2]) : NAN );
    // WRITE("AvgKinshipDegree_max", v[3]);
    // WRITE("AvgKinshipDegree_min", v[4]);

    STAT("nChildren");
    WRITE("nChildren_avg", v[1]);
    WRITE("nChildren_sd", v[2] > 0 ? sqrt(v[2]) : NAN );
    WRITE("nChildren_max", v[3]);
    WRITE("nChildren_min", v[4]);

    STAT_CND("PartnerDegreeBlood", "PartnerID", ">", -1.0);
    WRITE("PartnerDegreeBlood_avg", v[1]);
    WRITE("PartnerDegreeBlood_sd", v[2] > 0 ? sqrt(v[2]) : NAN );
    WRITE("PartnerDegreeBlood_max", v[3]);
    WRITE("PartnerDegreeBlood_min", v[4]);
    
    STAT_CND("d_social", "age", ">", 15);
    WRITE("d_social_avg", v[1]);
    WRITE("d_social_sd", v[2] > 0 ? sqrt(v[2]) : NAN );
    WRITE("d_social_max", v[3]);
    WRITE("d_social_min", v[4]);
    
}
REPORT_LOCAL_CLOCK_CND(0.02);
RESULT(statistics_ret)

EQUATION("Partner_Status")
/* Function. Is there currently a partner? 0: No, 1: Yes. Also check if partner is dead. */
double partner_status_ret = 0.0; //no partner yet
{
    double partner_id = V("PartnerID");
    if (partner_id > -1.0) {
        object* Partner = SEARCH_UID(partner_id);
        if (Partner == NULL) {
            partner_status_ret = -1.0; //partner dead
        }
        else {
            partner_status_ret = 1.0; //partner alive
#ifdef USE_LAT
            SET_LAT_PRIORITY(5);
            SET_LAT_COLOR(COLOR_LAT("Yellow"));
#endif
        }
    }
}
RESULT(partner_status_ret)

EQUATION("Potential_Partner")
/*  This is a function that reports to the caller if the callee is a suitable match,
    which implies also that the callee finds the caller suitable. */

if (c == p)
{
    PLOG("\nError! Caller equals callee");
    ABORT;
}
double potential_partner_ret = 0.0;
{
    double is_match = 0.0; //no
    bool alt_model = V("WD_alt_model") == 0.0 ? false : true; //define social pressure on share of people married, as in the original, or on share of people with children (true).
    double distance = RELATIVE_DISTANCE( DISTANCE(c) );
    bool is_free = alt_model ? ( V("Partner_Status") != 1 ) : ( V("Partner_Status") == 0 ); //in original model: only if unmarried. in alt model: if currently no partner.
    const int prohibited_degree = 5; //maximum degree of relatedness that is prohibited (i.e. most distant relatedness not allowed. 5 == cousinship)
    // bool not_of_kin = alt_model ? ( false == POP_CHECK_INCEST(c, p, prohibited_degree) ) : true;
    // PLOG(LSD_VALIDATE::track_sequence(t,p,c,var).c_str());
    // PLOG("\n Caller %p %g (%s) and callee %p %g (%s)",c, UIDS(c), POP_FEMALES(c) ? "female" : "male" ,p,UID, POP_FEMALE ? "female" : "male" );
    // if (POP_FEMALE != POP_FEMALES(c)){
    // PLOG("\nTest Pot partner %g and %g.", UID, UIDS(c));
    // PLOG("\n\t is free? : %s", is_free ? "Yes" : "No");
    // PLOG("\n\t different sex? : %s", POP_FEMALE != POP_FEMALES(c) ? "Yes" : "No");
    // PLOG("\n\t 2. in social distance of 1. : %s", !(distance > V("psearch_radius")) ? "Yes" : "No");
    // PLOG("\n\t 1. in social distance of 2. : %s", !(distance > VS(c, "psearch_radius")) ? "Yes" : "No");
    // PLOG("\n\t 2. in low age distance of 1. : %s", V("age_accept_low") <= POP_AGES(c) ? "Yes" : "No");
    // PLOG("\n\t 2. in high age distance of 1. : %s", POP_AGES(c) <= V("age_accept_high") ? "Yes" : "No");
    // PLOG("\n\t 1. in low age distance of 2. : %s", VS(c, "age_accept_low") <= POP_AGE ? "Yes" : "No");
    // PLOG("\n\t 1. in high age distance of 2. : %s", POP_AGE <= VS(c, "age_accept_high") ? "Yes" : "No");
    // PLOG("\n\t not of kin? : %s", (alt_model ? ( false == POP_CHECK_INCEST(c, p, prohibited_degree) ) : true ) ? "Yes" : "No");
    // }

    if (    true == is_free //can couple
            && POP_FEMALE != POP_FEMALES(c) //different sex
            && !( distance > V("psearch_radius") )  //within social/spatial range
            && ! (distance > VS(c, "psearch_radius") )
            && V("age_accept_low") <= POP_AGES(c) //within age range
            && POP_AGES(c) <= V("age_accept_high")
            && VS(c, "age_accept_low") <= POP_AGE
            && POP_AGE <= VS(c, "age_accept_high")
            //&& true == not_of_kin //are not of same kin
            && (alt_model ? ( false == POP_CHECK_INCEST(c, p, prohibited_degree) ) : true )
       ) {
        is_match = 1.0;
    }
    potential_partner_ret = is_match;
}
RESULT(potential_partner_ret)

EQUATION("Find_Partner")
/*  If the agent does not yet have a partner, it actively searches for a partner.
    In the original wedding ring model it searches for a partner if it didn't have one yet,
    i.e., if the partner died it will still not look for a new one.
    In the alternative model it searches for a partner if it is in fertility range only
    and if it doesn't have a partner.

    Note: Currently we do not break-up a partnership if one of the partners leaves fertility range.
*/
SET_LOCAL_CLOCK_RF
ADD_LOCAL_CLOCK_TRACKSEQUENCE
double find_partner_ret = 0.0;
{
    bool alt_model = V("WD_alt_model") == 0.0 ? false : true; //define social pressure on share of people married, as in the original, or on share of people with children (true).
    bool is_free = alt_model ? ( V("Partner_Status") != 1.0 ) : ( V("Partner_Status") == 0.0 ); //in original model: only if unmarried. in alt model: if currently no partner.

    int ndistance = ceil(V("Social_Pressure")*V("age_influence")*V("n_social") );    
    WRITE("rel_n_partner",(double)ndistance/V("n_social") ); //Analyse size of partner search

    if (true == is_free && ndistance > 0) {
        //randomly cycle through neighbours in partner range, mating with the first one that fits.
        // int count = 0;

        //RCYCLE_NEIGHBOUR(cur, p->label, ABSOLUTE_DISTANCE ( V("psearch_radius") ) ) { //first check distance
        NRCYCLE_NEIGHBOUR(cur, p->label, ndistance, -1 ) { //fast alternative
            
            // ++count;
            //Found a partner!
            if (V_CHEATS(cur, "Potential_Partner", p) == 1.0) {
                WRITE("PartnerID", UIDS(cur));
                WRITES(cur, "PartnerID", UID);

                double familyDegree = POP_FAMILY_DEGREE(p, cur);
                WRITE("PartnerDegreeBlood", familyDegree);
                WRITES(cur, "PartnerDegreeBlood", familyDegree);

                //Move to interception
                double size_s = V("nrelevant_others");
                double size_o = VS(cur, "nrelevant_others");
                double rel_pos = 0.5; //default
                double tot = size_s + size_o;
                if (tot > 0.0) {
                    rel_pos = size_s / tot;
                }
                //PLOG("\n Matching agents %s -- %s ", POP_INFO, POP_INFOS(cur) );
                POSITION_INTERCEPT(p, cur, rel_pos); //v[0] new x, v[1] new y
                TELEPORT_XY(v[0], v[1]);
                TELEPORT_XYS(cur, v[0], v[1]);
                //PLOG("\nMoved to position: %s ",GIS_INFO);



                //is_free = false;
                //PLOG("\nChecked a total of %i neighbours as partner to find one",count);
                break; //found a partner, end search
            }
        }
    }
    find_partner_ret = V("Partner_Status");
}
REPORT_LOCAL_CLOCK_CND(0.02);
RESULT(find_partner_ret)


EQUATION("AvgKinshipDegree")
/* Calculate the average Kinship Degree of the Agent with all living agents. */
double AvgKinshipDegree_ret = 0.0;
{
    double count;
    CYCLES(p->up, cur, "Agent") {
        if (p == cur) {
            continue; //skip self
        }
        ++count;
        AvgKinshipDegree_ret += POP_FAMILY_DEGREE(p, cur);
    }
    if (count > 0) {
        AvgKinshipDegree_ret /= count;
    }
}
RESULT(AvgKinshipDegree_ret)

/******************************************************************************/
/*  Template Model End                                                        */
/*----------------------------------------------------------------------------*/

/* User specific below */


EQUATION("TimeSpent")
/* Track the simulation time for each single step. */
if (t == 1)
    WRITE("LastTime", clock());

double delta = (double) ( clock() - V("LastTime") ) / CLOCKS_PER_SEC;
WRITE("LastTime", clock());
RESULT( delta )







MODELEND




void close_sim(void)
{
    PAJEK_POP_LINEAGE_SAVE
}


