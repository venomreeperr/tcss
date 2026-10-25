import React from "react";

export const Navbar = () => {
    const links = [
        {
            title: 'Guide',
            href: '#',
        },
        {
            title: 'Pricing',
            href: '#',
        },
        {
            title: 'Login',
            href: '#',
        },
    ];
    return (
        <nav className="navbar-root">
            <a href="#" className="logo">Fintech</a>
            <div className="links">
                {links.map((link) => (
                    <a className="link-items" key={link.title} href={link.href}>{link.title}</a>
                ))}
                <button className="btn btn-primary">Start free trial</button>
            </div>
        </nav>
    );
};

