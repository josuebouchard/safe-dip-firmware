#ifndef ENTRYPOINT_HPP
#define ENTRYPOINT_HPP

#ifdef __cplusplus
extern "C"
{
#endif

    // To be called after hardware initialization
    void setup();

    // To be called in while loop
    void loop();

#ifdef __cplusplus
}
#endif

#endif // ENTRYPOINT_HPP
